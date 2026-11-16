package ui;

import io.File;
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

    public static void drawHLine(int x, int y, int width, int color) {
        Native.writeBytes(0xa0000 + y * screenWidth + x, color, width);
    }

    public static void drawVLine(int x, int y, int height, int color) {
        int base = 0xa0000 + y * screenWidth + x;
        for (int r = 0; r < height; r++) {
            Native.writeByte(base + r * screenWidth, color);
        }
    }

    public static void drawRect(int x, int y, int width, int height, int color) {
        drawHLine(x, y, width, color);
        drawHLine(x, y + height - 1, width, color);
        drawVLine(x, y, height, color);
        drawVLine(x + width - 1, y, height, color);
    }

    public static void fillRect(int x, int y, int width, int height, int color) {
        int base = 0xa0000 + y * screenWidth + x;
        for (int r = 0; r < height; r++) {
            Native.writeBytes(base + r * screenWidth, color, width);
        }
    }

    public static boolean getPixels(int x, int y, int width, int height, String path) {
        int handle = File.open(path, 0x3c00);
        if (handle < 0) {
            return false;
        }
        File.close(handle);
        handle = File.open(path, 0x3d02);
        if (handle < 0) {
            return false;
        }
        byte[] row = new byte[width];
        int base = 0xa0000 + y * screenWidth;
        for (int r = 0; r < height; r++) {
            Native.readBytes(base + r * screenWidth, row, screenWidth, width, x);
            if (!File.write(handle, row)) {
                File.close(handle);
                return false;
            }
        }
        File.close(handle);
        return true;
    }

    public static boolean putPixels(int x, int y, int width, int height, String path) {
        int handle = File.open(path, 0x3d02);
        if (handle < 0) {
            return false;
        }
        int base = 0xa0000 + y * screenWidth;
        for (int r = 0; r < height; r++) {
            byte[] data = File.read(handle, width);
            if (data == null) {
                File.close(handle);
                return false;
            }
            Native.writeBytes(base + r * screenWidth, data, screenWidth, width, x, -1);
        }
        File.close(handle);
        return true;
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
