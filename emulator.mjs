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
            .replace(/%MAIN_CLASS%/g, 'demo.Desktop')
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
        return readFileSync(resolve(this.projectDir, 'emulator.html'), 'utf8');
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
