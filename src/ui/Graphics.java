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
        int base = 0xa0000 + y * screenWidth;
        int blockSize = 3200;
        int rowsPerBlock = blockSize / width;
        if (rowsPerBlock < 1) {
            rowsPerBlock = 1;
        }
        int remaining = height;
        int row = 0;
        while (remaining > 0) {
            int rowsThis = remaining < rowsPerBlock ? remaining : rowsPerBlock;
            int count = rowsThis * width;
            byte[] data = new byte[count];
            Native.readBytes(base + row * screenWidth, data, screenWidth, width, x);
            if (!File.write(handle, data)) {
                File.close(handle);
                return false;
            }
            remaining -= rowsThis;
            row += rowsThis;
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
        int blockSize = 3200;
        int rowsPerBlock = blockSize / width;
        if (rowsPerBlock < 1) {
            rowsPerBlock = 1;
        }
        int remaining = height;
        int row = 0;
        while (remaining > 0) {
            int rowsThis = remaining < rowsPerBlock ? remaining : rowsPerBlock;
            int count = rowsThis * width;
            byte[] data = File.read(handle, count);
            if (data == null) {
                File.close(handle);
                return false;
            }
            Native.writeBytes(base + row * screenWidth, data, screenWidth, width, x, -1);
            remaining -= rowsThis;
            row += rowsThis;
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
