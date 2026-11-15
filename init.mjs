#!/usr/bin/env node

import { execSync } from 'child_process';
import { mkdirSync, existsSync, readdirSync, rmSync, writeFileSync, readFileSync } from 'fs';
import { resolve } from 'path';
import SevenZipFactory from '7z-wasm';

const binDir = resolve(process.cwd(), 'bin');
mkdirSync(binDir, { recursive: true });

console.log('Installing components...\n');

async function downloadFile(url, path) {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const buffer = await response.arrayBuffer();
    writeFileSync(path, Buffer.from(buffer));
}

if (!existsSync(resolve(binDir, 'seabios.bin'))) {
    console.log('Downloading SeaBIOS...');
    await downloadFile('https://c.copy.sh/seabios.bin', resolve(binDir, 'seabios.bin'));
}

if (!existsSync(resolve(binDir, 'vgabios.bin'))) {
    console.log('Downloading VGA BIOS...');
    await downloadFile('https://c.copy.sh/vgabios.bin', resolve(binDir, 'vgabios.bin'));
}

if (!existsSync(resolve(binDir, 'freedos722.img'))) {
    console.log('Downloading FreeDOS 7.22...');
    await downloadFile('https://c.copy.sh/freedos722.img', resolve(binDir, 'freedos722.img'));
}

const downloads = [
    { name: 'i16butil.zip', url: 'https://gitlab.com/tkchia/build-ia16/uploads/373a2731c952daa65db62ad20bda94e2/i16butil.zip', check: 'I16BUTIL', mergeInto: 'I16GNU', lzma: true },
    { name: 'i16gcc.zip', url: 'https://gitlab.com/tkchia/build-ia16/uploads/b542b99990356b7e1bc79a1da17af2a6/i16gcc.zip', check: 'I16GNU', lzma: true },
    { name: 'i16newli.zip', url: 'https://gitlab.com/tkchia/build-ia16/uploads/c8524290ba3528b5fcb5c56af0dbd317/i16newli.zip', check: 'I16NEWLIB', mergeInto: 'I16GNU', lzma: true },
    { name: 'djgpp.zip', url: 'https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.2/devel/djgpp.zip', check: 'DJGPP' },
    { name: 'cwsdpmi.zip', url: 'https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/util/cwsdpmi.zip', check: 'CWSDPMI' },
];

for (const dl of downloads) {
    const dlPath = resolve(binDir, dl.name);
    if (!existsSync(dlPath)) {
        console.log(`Downloading ${dl.name}...`);
        await downloadFile(dl.url, dlPath);
    }
}

const develDir = resolve(binDir, 'devel');
mkdirSync(develDir, { recursive: true });

const tempDir = resolve(binDir, '.extract-temp');

const checkFiles = {
    I16BUTIL: () => existsSync(resolve(develDir, 'I16GNU', 'bin', 'i16ar.exe')),
    I16GNU: () => existsSync(resolve(develDir, 'I16GNU', 'bin', 'i16gcc.exe')),
    I16NEWLIB: () => existsSync(resolve(develDir, 'I16GNU', 'ia16-elf', 'lib', 'libc.a')),
    DJGPP: () => existsSync(resolve(develDir, 'DJGPP', 'lib')),
    CWSDPMI: () => existsSync(resolve(develDir, 'CWSDPMI', 'CWSDPMI.EXE')),
};

function copyDir(src, dst) {
    mkdirSync(dst, { recursive: true });
    for (const file of readdirSync(src, { withFileTypes: true })) {
        const srcPath = resolve(src, file.name);
        const dstPath = resolve(dst, file.name);
        if (file.isDirectory()) {
            copyDir(srcPath, dstPath);
        } else {
            const data = readFileSync(srcPath);
            writeFileSync(dstPath, data);
        }
    }
}

function copyWasmTree(sz, wasmPath, destPath) {
    mkdirSync(destPath, { recursive: true });
    const names = sz.FS.readdir(wasmPath);
    
    for (const name of names) {
        if (name === '.' || name === '..') continue;
        const fullWasmPath = wasmPath === '/' ? '/' + name : wasmPath + '/' + name;
        const fullDestPath = resolve(destPath, name);
        
        try {
            const stat = sz.FS.stat(fullWasmPath);
            if (sz.FS.isDir(stat.mode)) {
                copyWasmTree(sz, fullWasmPath, fullDestPath);
            } else {
                const data = sz.FS.readFile(fullWasmPath, { encoding: 'binary' });
                writeFileSync(fullDestPath, data);
            }
        } catch (e) {
            // silently skip
        }
    }
}

async function extractWithWasm(zipPath, tempSubDir) {
    const sz = await SevenZipFactory({ noFSInit: false });
    const zipBuffer = readFileSync(zipPath);
    
    sz.FS.writeFile('/archive.zip', zipBuffer);
    sz.callMain(['x', '/archive.zip', '-o/extracted', '-y']);
    
    const extractPath = resolve(tempDir, tempSubDir);
    copyWasmTree(sz, '/extracted', extractPath);
    return extractPath;
}

function extractWithPython(zipPath, tempSubDir) {
    const extractPath = resolve(tempDir, tempSubDir);
    mkdirSync(extractPath, { recursive: true });
    const pyCode = `
import zipfile
import sys
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
`;
    execSync(`python3 -c "${pyCode.replace(/"/g, '\\"')}" "${zipPath}" "${extractPath}"`);
    return extractPath;
}

for (const dl of downloads) {
    const targetName = dl.check;
    if (checkFiles[targetName] && checkFiles[targetName]()) {
        console.log(`✓ ${targetName} already present`);
        continue;
    }

    console.log(`Extracting ${dl.name}...`);

    if (existsSync(tempDir)) {
        rmSync(tempDir, { recursive: true, force: true });
    }

    try {
        const extractPath = dl.lzma 
            ? await extractWithWasm(resolve(binDir, dl.name), dl.name.replace('.zip', ''))
            : extractWithPython(resolve(binDir, dl.name), dl.name.replace('.zip', ''));
        
        const toplevel = readdirSync(extractPath);
        let sourcePath = extractPath;

        if (toplevel.includes('DEVEL')) {
            sourcePath = resolve(extractPath, 'DEVEL');
        } else if (toplevel.length === 1) {
            const firstDir = resolve(extractPath, toplevel[0]);
            const firstDirContents = readdirSync(firstDir);
            if (firstDirContents.includes('DEVEL')) {
                sourcePath = resolve(firstDir, 'DEVEL');
            }
        }

        if (dl.mergeInto) {
            const mergeTarget = dl.mergeInto;
            const src = resolve(sourcePath, 'I16GNU');
            const dst = resolve(develDir, mergeTarget);
            
            if (existsSync(src)) {
                if (!existsSync(dst)) {
                    mkdirSync(dst, { recursive: true });
                }
                
                function mergeDir(srcDir, dstDir) {
                    for (const file of readdirSync(srcDir, { withFileTypes: true })) {
                        const srcPath = resolve(srcDir, file.name);
                        const dstPath = resolve(dstDir, file.name);
                        if (file.isDirectory()) {
                            mkdirSync(dstPath, { recursive: true });
                            mergeDir(srcPath, dstPath);
                        } else {
                            const data = readFileSync(srcPath);
                            writeFileSync(dstPath, data);
                        }
                    }
                }
                
                mergeDir(src, dst);
            }
        } else if (targetName === 'CWSDPMI') {
            const src = resolve(extractPath, 'BIN');
            const dst = resolve(develDir, 'CWSDPMI');
            if (existsSync(src)) {
                copyDir(src, dst);
            }
        } else {
            const extracted = readdirSync(sourcePath);
            for (const dir of extracted) {
                const upperName = dir.toUpperCase();
                if (upperName !== targetName) continue;

                const src = resolve(sourcePath, dir);
                const stats = readdirSync(src);
                if (stats.length === 0) continue;

                const dst = resolve(develDir, upperName);
                if (existsSync(dst)) {
                    rmSync(dst, { recursive: true, force: true });
                }

                copyDir(src, dst);
            }
        }
    } catch (e) {
        console.error(`  Error: ${e.message}`);
        continue;
    }

    if (checkFiles[targetName] && checkFiles[targetName]()) {
        console.log(`✓ ${targetName} verified`);
    } else {
        console.warn(`⚠ ${targetName}: check skipped`);
    }
}

rmSync(tempDir, { recursive: true, force: true });

if (!existsSync(resolve(binDir, 'janino'))) {
    console.log('Downloading janino...');
    mkdirSync(resolve(binDir, 'janino'), { recursive: true });
    const v = '3.1.9';
    execSync(`curl -sL https://repo1.maven.org/maven2/org/codehaus/janino/janino/${v}/janino-${v}.jar -o ${resolve(binDir, 'janino', 'janino.jar')}`);
    execSync(`curl -sL https://repo1.maven.org/maven2/org/codehaus/commons-compiler/commons-compiler/${v}/commons-compiler-${v}.jar -o ${resolve(binDir, 'janino', 'commons-compiler.jar')}`);
}

const cacheFile = resolve(binDir, 'hdd-cache.bin');
if (!existsSync(cacheFile)) {
    console.log('\nBuilding HDD disk image...');
    const { createOrLoadHddImage } = await import('./image.mjs');
    await createOrLoadHddImage(process.cwd());
}

console.log('\n✓ Installation complete');
