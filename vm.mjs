import fs from 'node:fs/promises';
import { readFileSync } from 'fs';
import { V86 } from 'v86';
import { resolve } from 'path';
import { createOrLoadHddImage, mountFatPartition, copySources, copyBuildOutputs, copyData } from './image.mjs';

export class VM {
    constructor(config = {}) {
        this.projectDir = config.projectDir || process.cwd();
        this.binDir = resolve(this.projectDir, 'bin');
        this.emulator = null;
        this.configSys = {
            files: config.files ?? 255,
            buffers: config.buffers ?? 20,
            stacks: config.stacks ?? '9,256',
            device: config.device ?? 'HIMEM.SYS'
        };
    }

    generateConfigSys() {
        return [
            `DEVICE=${this.configSys.device}`,
            `FILES=${this.configSys.files}`,
            `BUFFERS=${this.configSys.buffers}`,
            `STACKS=${this.configSys.stacks}`
        ].join('\r\n');
    }

    async initialize(mainClass = 'demo.Desktop') {
        console.log('Loading FreeDOS image...');
        const floppyPath = resolve(this.binDir, 'freedos722.img');
        const floppyBuffer = Buffer.from(readFileSync(floppyPath));
        const floppyFs = await mountFatPartition(floppyBuffer, 0);

        console.log('Injecting AUTOEXEC.BAT...');
        const autoexecPath = resolve(this.projectDir, 'AUTOEXEC.BAT');
        let autoexecContent = '';
        try {
            autoexecContent = await fs.readFile(autoexecPath, 'utf8');
        } catch {
            throw new Error(`Failed to read AUTOEXEC.BAT from ${autoexecPath}`);
        }
        autoexecContent = autoexecContent.replace(/%MAIN_CLASS%/g, mainClass);

        await new Promise((res, rej) =>
            floppyFs.writeFile('/AUTOEXEC.BAT', autoexecContent, (e) => (e ? rej(e) : res()))
        );

        console.log('Injecting CONFIG.SYS...');
        const configContent = this.generateConfigSys();

        await new Promise((res, rej) =>
            floppyFs.writeFile('/CONFIG.SYS', configContent, (e) => (e ? rej(e) : res()))
        );

        console.log('Loading HDD image (may take up to 2 minutes on first run)...');
        const hddBuffer = await createOrLoadHddImage(this.projectDir);
        const hddFs = await mountFatPartition(hddBuffer, 63);
        await copySources(hddFs, this.projectDir);
        await copyBuildOutputs(hddFs, this.projectDir);
        await copyData(hddFs, this.projectDir);

        const toArrayBuffer = (buf) => {
            if (buf instanceof ArrayBuffer) return buf;
            return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
        };

        console.log('Starting V86 emulator...');
        const v86Dir = resolve(this.projectDir, 'node_modules/v86/build');
        const config = {
            wasm_path: resolve(v86Dir, 'v86.wasm'),
            bios: { url: resolve(this.binDir, 'seabios.bin') },
            vga_bios: { url: resolve(this.binDir, 'vgabios.bin') },
            fda: { buffer: toArrayBuffer(floppyBuffer) },
            hda: { buffer: toArrayBuffer(hddBuffer) },
            memory_size: 256 * 1024 * 1024,
            autostart: true,
        };

        this.emulator = new V86(config);

        return new Promise((resolve) => {
            this.emulator.add_listener('emulator-ready', resolve);
        });
    }

    destroy() {
        if (this.emulator) this.emulator.destroy();
    }
}
