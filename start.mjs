#!/usr/bin/env node

import { VM } from './vm.mjs';
import readline from 'node:readline';

const vm = new VM();
await vm.initialize();

console.log('\n[V86] FreeDOS VM booted. Interactive TTY active.\n');

let currentLine = '';
const promptLength = 4;

vm.emulator.add_listener('serial0-output-byte', (byte) => {
    if (byte === 10) {
        process.stdout.write('\r\n');
    } else if (byte === 13) {
        process.stdout.write('\r');
    } else {
        process.stdout.write(String.fromCharCode(byte));
    }
});

if (process.stdin.isTTY) {
    process.stdin.setRawMode(true);
}
process.stdin.resume();
process.stdin.setEncoding('utf8');

process.stdin.on('data', (key) => {
    if (key === '\u0003') {
        vm.destroy();
        process.exit(0);
    }

    if (key === '\r' || key === '\n') {
        process.stdout.write('\r\n');
        vm.emulator.serial0_send(currentLine + '\r');
        currentLine = '';
        readline.moveCursor(process.stdout, 0, -1);
        readline.cursorTo(process.stdout, promptLength);
        return;
    }

    if (key === '\u007f' || key === '\x7f' || key === '\b' || key.charCodeAt(0) === 127 || key.charCodeAt(0) === 8) {
        if (currentLine.length > 0) {
            currentLine = currentLine.slice(0, -1);
            readline.moveCursor(process.stdout, -1, 0);
            process.stdout.write(' ');
            readline.moveCursor(process.stdout, -1, 0);
        }
        return;
    }

    if (key.startsWith('\u001b') && key.length > 1) {
        return;
    }

    currentLine += key;
    process.stdout.write(key);
});