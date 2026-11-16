package ui;

import platform.Native;

public class Graphics {
    public static int width;

    public static void create(int vesaMode) {
        int[] regs = new int[9];
        if (vesaMode < 256) {
            regs[0] = vesaMode;
            width = 320;
        } else {
            regs[0] = 0x4f02;
            regs[1] = vesaMode;
            width = 640;
        }
        Native.interrupt(0x10, regs);
    }

    public static void putPixel(int x, int y, int color) {
        Native.writeByte(0xa0000 + y * width + x, color);
    }

    public static void putPixels(int x, int y, byte[] pixels) {
        Native.writeBytes(0xa0000 + y * width + x, pixels);
    }

    public static void putPixels(int x, int y, int color, int count) {
        Native.writeBytes(0xa0000 + y * width + x, color, count);
    }

    public static void destroy() {
        int[] regs = new int[9];
        regs[0] = 3;
        Native.interrupt(0x10, regs);
    }
}
