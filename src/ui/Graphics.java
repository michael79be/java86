package ui;

import platform.Native;

public class Graphics {
    public static int screenWidth;
    public static int screenHeight;

    public static void create(int vesaMode) {
        int[] regs = new int[9];
        if (0x13 == vesaMode) {
            regs[0] = vesaMode;
            screenWidth = 320;
            screenHeight = 200;
        } else if (0x101 == vesaMode) {
            regs[0] = 0x4f02;
            regs[1] = vesaMode;
            screenWidth = 640;
            screenHeight = 480;
        }
        Native.interrupt(0x10, regs);
    }

    public static void putPixel(int x, int y, int color) {
        Native.writeByte(0xa0000 + y * screenWidth + x, color);
    }

    public static void putPixels(int x, int y, byte[] pixels, int width) {
        Native.writeBytes(0xa0000 + y * screenWidth, pixels, screenWidth, width, x, -1);
    }

    public static void putPixels(int x, int y, int color, int count) {
        Native.writeBytes(0xa0000 + y * screenWidth + x, color, count);
    }
    
    public static void waitVbl() {
        while ((Native.readPort(0x3da) & 0x08) != 0);
        while ((Native.readPort(0x3da) & 0x08) == 0);
    }

    public static void destroy() {
        int[] regs = new int[9];
        regs[0] = 3;
        Native.interrupt(0x10, regs);
    }
}
