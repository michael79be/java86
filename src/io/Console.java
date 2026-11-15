package io;

import platform.Native;

public class Console {
    public static void print(String message) {
        byte[] bytes = Native.getBytes(message);
        int address = Native.getAddress(bytes);
        int[] regs = new int[9];
        regs[0] = 0x4000;
        regs[1] = 1;
        regs[2] = bytes.length;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        Native.interrupt(0x21, regs);
    }

    public static void println(String message) {
        print(message);
        print("\r\n");
    }

    public static String input() {
        byte[] buffer = new byte[129];
        buffer[0] = 127;
        int address = Native.getAddress(buffer);
        int[] regs = new int[9];
        regs[0] = 0x0a00;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        Native.interrupt(0x21, regs);
        int length = buffer[1];
        byte[] line = new byte[length];
        for (int i = 0; i < length; i++) {
            line[i] = buffer[i + 2];
        }
        return Native.toString(line);
    }
}
