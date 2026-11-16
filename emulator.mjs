#!/usr/bin/env node

import http from 'node:http';
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { createOrLoadHddImage, mountFatPartition, copySources, copyBuildOutputs, copyData } from './image.mjs';
import { Make } from './make.mjs';

class EmulatorServer {
    constructor(config = {}) {
        this.projectDir = config.projectDir || process.cwd();
        this.binDir = resolve(this.projectDir, 'bin');
        this.port = config.port || 8086;
        this.routes = new Map();
    }

    generateConfigSys() {
        return [
            'DEVICE=HIMEM.SYS',
            'FILES=255',
            'BUFFERS=20',
            'STACKS=9,256'
        ].join('\r\n');
    }

    writeFatFile(fatFs, path, content) {
        return new Promise((res, rej) =>
            fatFs.writeFile(path, content, (error) => (error ? rej(error) : res()))
        );
    }

    async prepareFloppy() {
        console.log('Preparing FreeDOS floppy...');
        const floppyBuffer = Buffer.from(readFileSync(resolve(this.binDir, 'freedos722.img')));
        const floppyFs = await mountFatPartition(floppyBuffer, 0);
        const autoexecContent = readFileSync(resolve(this.projectDir, 'AUTOEXEC.BAT'), 'utf8')
            .split(/\r?\n/)
            .filter(line => !line.trim().toUpperCase().startsWith('CTTY'))
            .join('\r\n');

        await this.writeFatFile(floppyFs, '/AUTOEXEC.BAT', autoexecContent);
        await this.writeFatFile(floppyFs, '/CONFIG.SYS', this.generateConfigSys());
        return floppyBuffer;
    }

    async prepareHdd() {
        console.log('Preparing HDD image...');
        const hddBuffer = await createOrLoadHddImage(this.projectDir);
        const hddFs = await mountFatPartition(hddBuffer, 63);
        const make = new Make(this.projectDir);
        await make.compileJavaSources();
        await copySources(hddFs, this.projectDir);
        await copyBuildOutputs(hddFs, this.projectDir);
        await copyData(hddFs, this.projectDir);
        return hddBuffer;
    }

    pageHtml() {
        return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>java86</title>
<style>
body { background: #202020; color: #d0d0d0; font-family: monospace; margin: 16px; }
#screen_container { display: inline-block; background: #000000; padding: 8px; }
</style>
</head>
<body>
<h1>java86 FreeDOS emulator</h1>
<div id="screen_container">
<div style="white-space: pre; font: 14px monospace; line-height: 14px"></div>
<canvas style="display: none"></canvas>
</div>
<script src="libv86.js"></script>
<script>
new V86({
    wasm_path: "v86.wasm",
    screen_container: document.getElementById("screen_container"),
    bios: { url: "seabios.bin" },
    vga_bios: { url: "vgabios.bin" },
    fda: { url: "floppy.img" },
    hda: { url: "hdd.img" },
    memory_size: 256 * 1024 * 1024,
    autostart: true
});
</script>
</body>
</html>
`;
    }

    async initialize() {
        const v86Dir = resolve(this.projectDir, 'node_modules/v86/build');

        this.routes.set('/', {
            type: 'text/html',
            body: Buffer.from(this.pageHtml())
        });
        this.routes.set('/libv86.js', {
            type: 'text/javascript',
            body: readFileSync(resolve(v86Dir, 'libv86.js'))
        });
        this.routes.set('/v86.wasm', {
            type: 'application/wasm',
            body: readFileSync(resolve(v86Dir, 'v86.wasm'))
        });
        this.routes.set('/seabios.bin', {
            type: 'application/octet-stream',
            body: readFileSync(resolve(this.binDir, 'seabios.bin'))
        });
        this.routes.set('/vgabios.bin', {
            type: 'application/octet-stream',
            body: readFileSync(resolve(this.binDir, 'vgabios.bin'))
        });
        this.routes.set('/floppy.img', {
            type: 'application/octet-stream',
            body: await this.prepareFloppy()
        });
        this.routes.set('/hdd.img', {
            type: 'application/octet-stream',
            body: await this.prepareHdd()
        });
    }

    start() {
        const server = http.createServer((request, response) => {
            const route = this.routes.get(request.url.split('?')[0]);
            if (!route) {
                response.writeHead(404, { 'Content-Type': 'text/plain' });
                response.end('Not found');
                return;
            }
            response.writeHead(200, {
                'Content-Type': route.type,
                'Content-Length': route.body.length
            });
            response.end(route.body);
        });
        server.listen(this.port, () => {
            console.log(`Emulator ready: http://localhost:${this.port}/`);
        });
    }
}

const emulator = new EmulatorServer({ projectDir: process.cwd() });
await emulator.initialize();
emulator.start();
