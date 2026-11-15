#!/usr/bin/env node

import { VM } from './vm.mjs';
import { execSync } from 'child_process';
import { resolve, extname, basename, delimiter, relative, sep } from 'path';
import { writeFileSync, mkdirSync, readdirSync, readFileSync } from 'fs';
import { existsSync } from 'fs';

class BuildVM extends VM {
    constructor(config = {}) {
        super(config);
        this.sourceFiles = config.sourceFiles || [];
        this.compileFile = config.compileFile;
        this.hddBuffer = null;
        this.buildLog = [];
        this.floppyFs = null;
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

        console.log(`[BUILD] Compiling ${javaFiles.length} Java file(s) with Janino...`);
        execSync(command, { cwd: this.projectDir, stdio: 'inherit' });

        const classFiles = collectFiles(buildDir, '.class').map((file) => relative(buildDir, file));
        if (classFiles.length === 0) {
            throw new Error(`No Java class files found in ${buildDir}`);
        }

        await this.createClassesJar(buildDir, classFiles);
    }

    async createClassesJar(buildDir, classFiles) {
        const SevenZipFactory = (await import('7z-wasm')).default;
        const sevenZip = await SevenZipFactory({ noFSInit: false });
        const archiveRoot = '/classes';
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
        const jarPath = resolve(buildDir, 'CLASSES.JAR');
        console.log(`[BUILD] Creating uncompressed ${jarPath}...`);
        sevenZip.callMain(['a', '-tzip', '/CLASSES.JAR', ...archiveFiles, '-mx=0']);
        writeFileSync(jarPath, Buffer.from(sevenZip.FS.readFile('/CLASSES.JAR')));
    }

    async setupSourceFiles() {
        const { mountFatPartition } = await import('./image.mjs');
        const hddFs = await mountFatPartition(this.hddBuffer, 63);

        console.log('Copying source files to HDD...');
        await new Promise((res, rej) =>
            hddFs.mkdir('/SRC', (e) => {
                if (e && e.code !== 'EEXIST' && !e.toString().includes('exists')) rej(e);
                else res();
            })
        );

        for (const file of this.sourceFiles) {
            const { readFileSync } = await import('fs');
            const fileContent = readFileSync(file);
            const filename = basename(file).toLowerCase();
            const dosPath = `/SRC/${filename}`;

            await new Promise((res, rej) =>
                hddFs.writeFile(dosPath, fileContent, (e) => (e ? rej(e) : res()))
            );
            console.log(`  ${filename}`);
        }
    }

    generateBuildAutoexec() {
        const commands = [
            'CTTY COM1',
            'SET PATH=C:\\DOS;C:\\TOOLS;C:\\I16GNU\\BIN;C:\\DJGPP\\BIN',
            'SET TEMP=C:\\TEMP',
            'SET TMP=C:\\TEMP',
            'SET DJGPP=C:\\DJGPP\\DJGPP.ENV',
            'IF EXIST C:\\TOOLS\\VBMOUSE.EXE C:\\TOOLS\\VBMOUSE.EXE',
            'C:',
            'cd \\SRC'
        ];

        const filename = basename(this.compileFile);
        const comName = filename.replace(extname(filename), '.com');
        commands.push(`i16gcc -mcmodel=tiny ${filename} -o ${comName}`);

        commands.push('DIR *.COM');
        commands.push()

        return commands.join('\r\n');
    }

    async initialize() {
        console.log('Loading FreeDOS image...');
        const floppyPath = resolve(this.binDir, 'freedos722.img');
        const { readFileSync } = await import('fs');
        const floppyBuffer = Buffer.from(readFileSync(floppyPath));
        const { mountFatPartition } = await import('./image.mjs');
        this.floppyFs = await mountFatPartition(floppyBuffer, 0);

        console.log('Loading HDD image...');
        const { createOrLoadHddImage } = await import('./image.mjs');
        this.hddBuffer = await createOrLoadHddImage(this.projectDir);

        await this.setupSourceFiles();

        console.log('Generating build AUTOEXEC.BAT...');
        const autoexecContent = this.generateBuildAutoexec();

        const debugDir = resolve(this.projectDir, 'logs');
        if (!existsSync(debugDir)) {
            mkdirSync(debugDir, { recursive: true });
        }
        const debugPath = resolve(debugDir, 'AUTOEXEC.BAT.debug');
        writeFileSync(debugPath, autoexecContent);
        console.log(`DEBUG: Saved AUTOEXEC.BAT to ${debugPath}`);

        await new Promise((res, rej) =>
            this.floppyFs.writeFile('/AUTOEXEC.BAT', autoexecContent, (e) => (e ? rej(e) : res()))
        );

        console.log('Injecting CONFIG.SYS...');
        const configContent = this.generateConfigSys();

        await new Promise((res, rej) =>
            this.floppyFs.writeFile('/CONFIG.SYS', configContent, (e) => (e ? rej(e) : res()))
        );

        const toArrayBuffer = (buf) => {
            if (buf instanceof ArrayBuffer) return buf;
            return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
        };

        console.log('Starting V86 emulator...');
        const v86Dir = resolve(this.projectDir, 'node_modules/v86/build');
        const { V86 } = await import('v86');
        const hddArrayBuffer = toArrayBuffer(this.hddBuffer);
        this.hddBuffer = Buffer.from(hddArrayBuffer);
        const vmConfig = {
            wasm_path: resolve(v86Dir, 'v86.wasm'),
            bios: { url: resolve(this.binDir, 'seabios.bin') },
            vga_bios: { url: resolve(this.binDir, 'vgabios.bin') },
            fda: { buffer: toArrayBuffer(floppyBuffer) },
            hda: { buffer: hddArrayBuffer },
            memory_size: 256 * 1024 * 1024,
            autostart: true,
        };

        this.emulator = new V86(vmConfig);

        return new Promise((resolve) => {
            this.emulator.add_listener('emulator-ready', resolve);
        });
    }

    async retrieveCompiledFiles() {
        console.log('Retrieving compiled files...');

        const { mountFatPartition } = await import('./image.mjs');
        const hddFsNew = await mountFatPartition(this.hddBuffer, 63);

        const releaseDir = resolve(this.projectDir, 'release');
        if (!existsSync(releaseDir)) {
            mkdirSync(releaseDir, { recursive: true });
        }

        const comFiles = [];

        const comName = basename(this.compileFile).replace(extname(this.compileFile), '.com');
        const dosPath = `/SRC/${comName}`;
        const dosPathUpper = `/SRC/${comName.toUpperCase()}`;

        try {
            await new Promise((res, rej) => {
                hddFsNew.readFile(dosPath, (err, data) => {
                    if (err) {
                        hddFsNew.readFile(dosPathUpper, (err2, data2) => {
                            if (err2) {
                                this.buildLog.push(`  ${comName}: not found`);
                                res();
                            } else {
                                const hostPath = resolve(releaseDir, comName);
                                writeFileSync(hostPath, data2);
                                this.buildLog.push(`  ${comName} -> ${hostPath}`);
                                comFiles.push(hostPath);
                                res();
                            }
                        });
                    } else {
                        const hostPath = resolve(releaseDir, comName);
                        writeFileSync(hostPath, data);
                        this.buildLog.push(`  ${comName} -> ${hostPath}`);
                        comFiles.push(hostPath);
                        res();
                    }
                });
            });
        } catch (e) {
            this.buildLog.push(`  Error: ${e.message}`);
        }

        return comFiles;
    }

    async saveLogs() {
        const logsDir = resolve(this.projectDir, 'logs');
        if (!existsSync(logsDir)) {
            mkdirSync(logsDir, { recursive: true });
        }

        const logPath = resolve(logsDir, 'build.log');
        const logContent = this.buildLog.join('\n');
        writeFileSync(logPath, logContent);

        return logPath;
    }
}

async function main() {
    const args = process.argv.slice(2);
    const srcDir = resolve(process.cwd(), 'src');

    if (args.length !== 1) {
        console.error('[BUILD] Usage: npm run build -- src/hello.c');
        process.exit(1);
    }

    const sourceFiles = readdirSync(srcDir, { withFileTypes: true })
        .filter(entry => entry.isFile() && extname(entry.name).toLowerCase() === '.c')
        .map(entry => resolve(srcDir, entry.name));

    if (sourceFiles.length === 0) {
        console.error('[BUILD] No .c files found in src/');
        process.exit(1);
    }

    const compileFile = resolve(args[0]);
    if (!sourceFiles.includes(compileFile)) {
        console.error(`[BUILD] Source file must be a .c file directly inside src/: ${args[0]}`);
        process.exit(1);
    }

    console.log(`[BUILD] Copying ${sourceFiles.length} C file(s); compiling ${basename(compileFile)}...\n`);

    const builder = new BuildVM({ projectDir: process.cwd(), sourceFiles, compileFile });

    try {
        await builder.compileJavaSources();
        await builder.initialize();

        let currentLine = '';
        let dirOutputComplete = false;

        builder.emulator.add_listener('serial0-output-byte', (byte) => {
            const char = String.fromCharCode(byte);
            process.stdout.write(char);

            currentLine += char;
            if (byte === 10 || byte === 13) {
                if (currentLine.trim()) {
                    builder.buildLog.push(currentLine.trim());
                }
                if (currentLine.includes('C:\\SRC>') && builder.buildLog.some(l => l.includes('.COM'))) {
                    dirOutputComplete = true;
                }
                currentLine = '';
            }
        });

        await new Promise(resolve => {
            const check = setInterval(() => {
                if (dirOutputComplete) {
                    clearInterval(check);
                    resolve();
                }
            }, 100);
            setTimeout(() => {
                clearInterval(check);
                resolve();
            }, 30000);
        });

        await new Promise(resolve => setTimeout(resolve, 1000));

        const compiled = await builder.retrieveCompiledFiles();

        builder.destroy();

        const logPath = await builder.saveLogs();
        console.log(`\n[BUILD] Compiled ${compiled.length} .COM file(s) successfully.`);
        console.log(`[BUILD] Log saved: ${logPath}\n`);
        process.exit(0);

    } catch (err) {
        console.error('[BUILD] Error:', err.message);

        const logPath = await builder.saveLogs();
        console.error(`[BUILD] Log saved: ${logPath}\n`);

        builder.destroy();
        process.exit(1);
    }
}

main();
