import { resolve, extname, delimiter, relative, sep } from 'path'
import { readdirSync, readFileSync, writeFileSync, mkdirSync, existsSync, statSync } from 'fs'
import { execSync } from 'child_process'
import { createHash } from 'crypto'

export class Make {
    constructor(projectDir) {
        this.projectDir = projectDir;
    }

    async compileJavaSources() {
        const srcDir = resolve(this.projectDir, 'src');
        const collectFiles = (directory, extension) =>
            readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
                const file = resolve(directory, entry.name);
                if (entry.isDirectory()) return collectFiles(file, extension);
                return entry.isFile() && extname(entry.name).toLowerCase() === extension ? [file] : [];
            });
        const javaFiles = collectFiles(srcDir, '.java');

        if (javaFiles.length === 0) return;

        const janinoDir = resolve(this.projectDir, 'bin', 'janino');
        const jars = [
            resolve(janinoDir, 'janino.jar'),
            resolve(janinoDir, 'commons-compiler.jar')
        ];
        if (!jars.every(existsSync)) {
            throw new Error(`Janino JARs not found in ${janinoDir}`);
        }

        const buildDir = resolve(this.projectDir, 'build');
        mkdirSync(buildDir, { recursive: true });

        const releaseDir = resolve(this.projectDir, 'release');
        mkdirSync(releaseDir, { recursive: true });

        const staleFiles = javaFiles.filter(javaFile => {
            const rel = relative(srcDir, javaFile);
            const classFile = resolve(buildDir, rel.replace(/\.java$/, '.class'));
            if (!existsSync(classFile)) return true;
            return statSync(javaFile).mtimeMs > statSync(classFile).mtimeMs;
        });

        if (staleFiles.length === 0) {
            console.log('[BUILD] No Java sources changed, skipping compilation');
            return;
        }

        console.log(`[BUILD] ${staleFiles.length} of ${javaFiles.length} Java source(s) changed, recompiling...`);

        const classHashesBefore = collectClassHashes(buildDir);

        const quote = (value) => `'${value.replace(/'/g, "'\\''")}'`;
        const command = [
            'java',
            '-cp',
            quote(jars.join(delimiter)),
            'org.codehaus.commons.compiler.samples.CompilerDemo',
            '-d',
            quote(buildDir),
            ...javaFiles.map(quote)
        ].join(' ');
        execSync(command, { cwd: this.projectDir, stdio: 'inherit' });

        const classHashesAfter = collectClassHashes(buildDir);

        const changedClasses = new Set();
        for (const [rel, hashAfter] of classHashesAfter) {
            const hashBefore = classHashesBefore.get(rel);
            if (hashBefore === undefined || hashBefore !== hashAfter) {
                changedClasses.add(rel);
            }
        }

        const classFiles = collectFiles(buildDir, '.class').map((file) => relative(buildDir, file));
        if (classFiles.length === 0) {
            throw new Error(`No Java class files found in ${buildDir}`);
        }

        const packages = new Map();
        for (const file of classFiles) {
            const parts = file.split(sep);
            if (parts.length < 2) {
                throw new Error(`Class without package is not supported: ${file}`);
            }
            const packageName = parts[0];
            if (!packages.has(packageName)) packages.set(packageName, []);
            packages.get(packageName).push(file);
        }

        for (const [packageName, files] of packages) {
            const jarName = `${packageName.toUpperCase()}.JAR`;
            const jarPath = resolve(releaseDir, jarName);
            const hasChanged = files.some(file => changedClasses.has(file));
            if (!hasChanged && existsSync(jarPath)) {
                console.log(`[BUILD] ${jarName} up to date`);
                continue;
            }
            await this.createPackageJar(buildDir, releaseDir, packageName, files);
        }
    }

    async createPackageJar(buildDir, jarDir, packageName, classFiles) {
        const SevenZipFactory = (await import('7z-wasm')).default;
        const sevenZip = await SevenZipFactory({ noFSInit: false });
        const archiveRoot = '/classes';
        const jarName = `${packageName.toUpperCase()}.JAR`;
        sevenZip.FS.mkdir(archiveRoot);

        const archiveFiles = classFiles.map((file) => file.split(sep).join('/'));
        const directories = new Set();
        for (const file of archiveFiles) {
            const parts = file.split('/');
            let directory = archiveRoot;
            for (const part of parts.slice(0, -1)) {
                directory += `/${part}`;
                directories.add(directory);
            }
        }

        for (const directory of [...directories].sort((left, right) => left.length - right.length)) {
            sevenZip.FS.mkdir(directory);
        }
        for (let index = 0; index < archiveFiles.length; index++) {
            const file = archiveFiles[index];
            sevenZip.FS.writeFile(
                `${archiveRoot}/${file}`,
                readFileSync(resolve(buildDir, classFiles[index]))
            );
        }

        sevenZip.FS.chdir(archiveRoot);
        const jarPath = resolve(jarDir, jarName);
        console.log(`[BUILD] Creating uncompressed ${jarPath}...`);
        sevenZip.callMain(['a', '-tzip', `/${jarName}`, ...archiveFiles, '-mx=0']);
        writeFileSync(jarPath, Buffer.from(sevenZip.FS.readFile(`/${jarName}`)));
    }
}

const make = new Make(process.cwd());
await make.compileJavaSources();

function collectClassHashes(buildDir) {
    const hashes = new Map();
    const collectFiles = (directory) =>
        readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
            const file = resolve(directory, entry.name);
            if (entry.isDirectory()) return collectFiles(file);
            return entry.isFile() && entry.name.toLowerCase().endsWith('.class') ? [file] : [];
        });
    for (const file of collectFiles(buildDir)) {
        const rel = relative(buildDir, file);
        hashes.set(rel, createHash('sha256').update(readFileSync(file)).digest('hex'));
    }
    return hashes;
}
