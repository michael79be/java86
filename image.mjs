import fs from 'node:fs/promises';
import { readFileSync, readdirSync, existsSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import fatfs from 'fatfs';

function createBlankFat16HddBuffer(sizeInMB = 64) {
    const sectorSize = 512;
    const totalSectors = (sizeInMB * 1024 * 1024) / sectorSize;
    const partitionStartLBA = 63;
    const partitionSectors = totalSectors - partitionStartLBA;

    const buffer = Buffer.alloc(totalSectors * sectorSize, 0);

    const partOffset = 446;
    buffer[partOffset + 0] = 0x80;
    buffer[partOffset + 1] = 0x01;
    buffer[partOffset + 2] = 0x01;
    buffer[partOffset + 3] = 0x00;
    buffer[partOffset + 4] = 0x06;
    buffer[partOffset + 5] = 0xFE;
    buffer[partOffset + 6] = 0xFF;
    buffer[partOffset + 7] = 0xFF;
    buffer.writeUInt32LE(partitionStartLBA, partOffset + 8);
    buffer.writeUInt32LE(partitionSectors, partOffset + 12);
    buffer.writeUInt16LE(0xAA55, 510);

    const vbrOffset = partitionStartLBA * sectorSize;
    const sectorsPerCluster = 4;
    const reservedSectors = 1;
    const numFats = 2;
    const rootEntries = 512;
    const sectorsPerFat = 256;

    buffer.set([0xEB, 0x3C, 0x90], vbrOffset + 0);
    buffer.write('MSDOS5.0', vbrOffset + 3, 'ascii');
    buffer.writeUInt16LE(sectorSize, vbrOffset + 11);
    buffer[vbrOffset + 13] = sectorsPerCluster;
    buffer.writeUInt16LE(reservedSectors, vbrOffset + 14);
    buffer[vbrOffset + 16] = numFats;
    buffer.writeUInt16LE(rootEntries, vbrOffset + 17);
    buffer.writeUInt16LE(
        partitionSectors < 65536 ? partitionSectors : 0,
        vbrOffset + 19
    );
    buffer[vbrOffset + 21] = 0xF8;
    buffer.writeUInt16LE(sectorsPerFat, vbrOffset + 22);
    buffer.writeUInt16LE(63, vbrOffset + 24);
    buffer.writeUInt16LE(16, vbrOffset + 26);
    buffer.writeUInt32LE(partitionStartLBA, vbrOffset + 28);
    buffer.writeUInt32LE(
        partitionSectors >= 65536 ? partitionSectors : 0,
        vbrOffset + 32
    );
    buffer[vbrOffset + 36] = 0x80;
    buffer[vbrOffset + 38] = 0x29;
    buffer.writeUInt32LE(0x12345678, vbrOffset + 39);
    buffer.write('FREEDOS_HDD', vbrOffset + 43, 'ascii');
    buffer.write('FAT16   ', vbrOffset + 54, 'ascii');
    buffer.writeUInt16LE(0xAA55, vbrOffset + 510);

    for (let f = 0; f < numFats; f++) {
        const fatOffset = (partitionStartLBA + reservedSectors + f * sectorsPerFat) * sectorSize;
        buffer.writeUInt16LE(0xFFF8, fatOffset + 0);
        buffer.writeUInt16LE(0xFFFF, fatOffset + 2);
    }

    return { buffer, partitionStartLBA };
}

async function mountFatPartition(imageBuffer, lbaOffset = 0) {
    const sectorSize = 512;
    const driver = {
        sectorSize,
        numSectors: Math.floor(imageBuffer.length / sectorSize) - lbaOffset,
        readSectors(sector, dest, cb) {
            const offset = (sector + lbaOffset) * sectorSize;
            imageBuffer.copy(dest, 0, offset, offset + dest.length);
            cb(null);
        },
        writeSectors(sector, data, cb) {
            const offset = (sector + lbaOffset) * sectorSize;
            data.copy(imageBuffer, offset);
            cb(null);
        }
    };

    const disk = fatfs.createFileSystem(driver);
    await new Promise((resolve, reject) => {
        disk.on('ready', resolve);
        disk.on('error', reject);
    });
    return disk;
}

async function writeFileToFat(fs, path, content) {
    return new Promise((resolve, reject) => {
        fs.writeFile(path, content, (err) => {
            if (err) reject(err);
            else resolve();
        });
    });
}

async function mkdirOnFat(fs, path) {
    return new Promise((resolve, reject) => {
        fs.mkdir(path, (err) => {
            if (err && err.code !== 'EXIST' && err.code !== 'EEXIST') reject(err);
            else resolve();
        });
    });
}

export async function createOrLoadHddImage(projectDir = process.cwd()) {
    const binDir = resolve(projectDir, 'bin');
    const cacheFile = resolve(binDir, 'hdd-cache.bin');

    if (existsSync(cacheFile)) {
        console.log('Loading cached HDD image from disk...');
        return await fs.readFile(cacheFile);
    }

    console.log('Creating HDD image (this may take 1-2 minutes)...');
    const { buffer: hddBuffer, partitionStartLBA } = createBlankFat16HddBuffer(64);
    const hddFs = await mountFatPartition(hddBuffer, partitionStartLBA);

    await populateHdd(hddFs, projectDir);

    console.log('Saving HDD image to disk...');
    writeFileSync(cacheFile, hddBuffer);

    return hddBuffer;
}

async function populateHdd(fs, projectDir) {
    await copySources(fs, projectDir);
    await copyDevel(fs, projectDir);
    await copyTools(fs, projectDir);
}

async function copySources(fs, projectDir) {
    const srcDir = resolve(projectDir, 'src');
    if (!existsSync(srcDir)) return;

    console.log('Copying source files to /SRC/...');
    await mkdirOnFat(fs, '/SRC');

    const allDirs = new Set();
    const allFiles = [];

    const walkDir = (dir, prefix = '') => {
        try {
            const entries = readdirSync(dir, { withFileTypes: true });
            for (const entry of entries) {
                const fullPath = resolve(dir, entry.name);
                const dosPath = prefix === ''
                    ? `/SRC/${entry.name}`.toUpperCase()
                    : `${prefix}/${entry.name}`.toUpperCase();

                if (entry.isDirectory()) {
                    allDirs.add(dosPath);
                    walkDir(fullPath, dosPath);
                } else if (entry.isFile()) {
                    allFiles.push({ src: fullPath, dst: dosPath });
                }
            }
        } catch (e) {
        }
    };

    walkDir(srcDir);

    for (const dir of allDirs) {
        await mkdirOnFat(fs, dir);
    }

    for (const file of allFiles) {
        const content = readFileSync(file.src);
        await writeFileToFat(fs, file.dst, content);
    }
}

async function copyDevel(fs, projectDir) {
    const develDir = resolve(projectDir, 'bin', 'devel');
    if (!existsSync(develDir)) return;

    console.log('Copying compiler and tools to /I16GNU/ and /DJGPP/...');
    await mkdirOnFat(fs, '/I16GNU');
    await mkdirOnFat(fs, '/DJGPP');

    const skipDirs = new Set(['SHARE', '.DS_STORE']);
    const allDirs = new Set();
    const allFiles = [];

    const walkDir = (dir, prefix = '') => {
        try {
            const entries = readdirSync(dir, { withFileTypes: true });
            for (const entry of entries) {
                if (skipDirs.has(entry.name.toUpperCase())) continue;
                if (entry.name.startsWith('.')) continue;

                const fullPath = resolve(dir, entry.name);
                const isDjgpp = entry.name.toUpperCase() === 'DJGPP';
                const basePath = isDjgpp ? '/DJGPP' : '/I16GNU';
                const dosPath = prefix === ''
                    ? `${basePath}`.toUpperCase()
                    : `${prefix}/${entry.name}`.toUpperCase();

                if (entry.isDirectory()) {
                    if (prefix === '') {
                        walkDir(fullPath, basePath);
                    } else {
                        allDirs.add(dosPath);
                        walkDir(fullPath, dosPath);
                    }
                } else if (entry.isFile()) {
                    if (prefix === '') {
                        allFiles.push({ src: fullPath, dst: `${basePath}/${entry.name}`.toUpperCase() });
                    } else {
                        allFiles.push({ src: fullPath, dst: dosPath });
                    }
                }
            }
        } catch (e) {
        }
    };

    walkDir(develDir);

    const cwsdpmiPath = resolve(develDir, 'CWSDPMI', 'CWSDPMI.EXE');
    if (existsSync(cwsdpmiPath)) {
        allFiles.push({ src: cwsdpmiPath, dst: '/DJGPP/BIN/CWSDPMI.EXE' });
    }

    for (const dir of allDirs) {
        await mkdirOnFat(fs, dir);
    }

    let count = 0;
    for (const file of allFiles) {
        try {
            const content = readFileSync(file.src);
            await writeFileToFat(fs, file.dst, content);
            count++;
            if (count % 100 === 0) console.log(`  Copied ${count} files...`);
        } catch (e) {
        }
    }
    console.log(`  Total: ${count} files from toolchain`);
}

async function copyTools(fs, projectDir) {
    const binDir = resolve(projectDir, 'bin');
    const mkeyb = resolve(binDir, 'mkeyb.exe');
    const vbmouse = resolve(binDir, 'vbmouse.exe');

    if (!existsSync(mkeyb) && !existsSync(vbmouse)) return;

    console.log('Copying tools to /TOOLS/...');
    await mkdirOnFat(fs, '/TOOLS');

    if (existsSync(mkeyb)) {
        const content = readFileSync(mkeyb);
        await writeFileToFat(fs, '/TOOLS/MKEYB.EXE', content);
    }
    if (existsSync(vbmouse)) {
        const content = readFileSync(vbmouse);
        await writeFileToFat(fs, '/TOOLS/VBMOUSE.EXE', content);
    }
}

export { mountFatPartition };
