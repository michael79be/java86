package io;

import platform.Native;

public class File {
    public static byte[] dta;

    static int[] pathCall(String path, int function, int cx) {
        byte[] name = Native.getBytes(path);
        byte[] asciiz = new byte[name.length + 1];
        for (int i = 0; i < name.length; i++) {
            asciiz[i] = name[i];
        }
        int address = Native.getAddress(asciiz);
        int[] regs = new int[9];
        regs[0] = function;
        regs[2] = cx;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        return Native.interrupt(0x21, regs);
    }

    static int open(String path, int function) {
        int[] regs = pathCall(path, function, 0);
        if ((regs[8] & 1) != 0) {
            return -1;
        }
        return regs[0];
    }

    static void close(int handle) {
        int[] regs = new int[9];
        regs[0] = 0x3e00;
        regs[1] = handle;
        Native.interrupt(0x21, regs);
    }

    static int seek(int handle, int origin) {
        int[] regs = new int[9];
        regs[0] = 0x4200 + origin;
        regs[1] = handle;
        regs[2] = 0;
        regs[3] = 0;
        regs = Native.interrupt(0x21, regs);
        if ((regs[8] & 1) != 0 || regs[3] != 0) {
            return -1;
        }
        return regs[0];
    }

    public static byte[] read(String path) {
        int handle = open(path, 0x3d00);
        if (handle < 0) {
            return null;
        }
        int size = seek(handle, 2);
        if (size < 0 || seek(handle, 0) < 0) {
            close(handle);
            return null;
        }
        byte[] data = new byte[size];
        int address = Native.getAddress(data);
        int[] regs = new int[9];
        regs[0] = 0x3f00;
        regs[1] = handle;
        regs[2] = size;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        regs = Native.interrupt(0x21, regs);
        close(handle);
        if ((regs[8] & 1) != 0 || regs[0] != size) {
            return null;
        }
        return data;
    }

    public static boolean write(String path, byte[] data) {
        int handle = open(path, 0x3c00);
        if (handle < 0) {
            return false;
        }
        int address = Native.getAddress(data);
        int[] regs = new int[9];
        regs[0] = 0x4000;
        regs[1] = handle;
        regs[2] = data.length;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        regs = Native.interrupt(0x21, regs);
        close(handle);
        if ((regs[8] & 1) != 0 || regs[0] != data.length) {
            return false;
        }
        return true;
    }

    static void setDta() {
        int address = Native.getAddress(dta);
        int[] regs = new int[9];
        regs[0] = 0x1a00;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        Native.interrupt(0x21, regs);
    }

    static String dtaName() {
        int length = 0;
        while (length < 13 && dta[30 + length] != 0) {
            length++;
        }
        byte[] name = new byte[length];
        for (int i = 0; i < length; i++) {
            name[i] = dta[30 + i];
        }
        return Native.toString(name);
    }

    public static String findFirst(String pattern) {
        dta = new byte[64];
        setDta();
        int[] regs = pathCall(pattern, 0x4e00, 0x10);
        if ((regs[8] & 1) != 0) {
            return null;
        }
        return dtaName();
    }

    public static String findNext() {
        if (dta == null) {
            return null;
        }
        setDta();
        int[] regs = new int[9];
        regs[0] = 0x4f00;
        regs = Native.interrupt(0x21, regs);
        if ((regs[8] & 1) != 0) {
            return null;
        }
        return dtaName();
    }

    public static boolean changeDir(String path) {
        int[] regs = pathCall(path, 0x3b00, 0);
        return (regs[8] & 1) == 0;
    }
}
