package ui;

import io.File;
import platform.Native;

public class BMP {
    public static int file;
    public static int width;
    public static int height;
    public static int offset;
    public static int stride;

    public static int open(String path) {
        if (file > 0) {
            close();
        }
        file = File.open(path, 0x3d00);
        if (file < 0) {
            return -1;
        }
        byte[] header = new byte[54];
        int headerAddress = Native.getAddress(header);
        int[] regs = new int[9];
        regs[0] = 0x3f00;
        regs[1] = file;
        regs[2] = 54;
        regs[3] = headerAddress & 15;
        regs[6] = headerAddress >> 4;
        Native.interrupt(0x21, regs);
        offset = (header[10] & 0xff) | ((header[11] & 0xff) << 8) |
            ((header[12] & 0xff) << 16) | ((header[13] & 0xff) << 24);
        width = (header[18] & 0xff) | ((header[19] & 0xff) << 8);
        height = (header[22] & 0xff) | ((header[23] & 0xff) << 8);
        stride = (width + 3) & ~3;
        return file;
    }

    public static void close() {
        if (file <= 0) {
            return;
        }
        File.close(file);
        file = -1;
    }

    public static void read(byte[] row, int y) {
        if (null == row || file <= 0) {
            return;
        }
        int fileRow = height - 1 - y;
        int fileOffset = offset + fileRow * stride;
        int[] regs = new int[9];
        regs[0] = 0x4200;
        regs[1] = file;
        regs[2] = fileOffset >> 16;
        regs[3] = fileOffset & 0xffff;
        Native.interrupt(0x21, regs);
        int address = Native.getAddress(row);
        regs = new int[9];
        regs[0] = 0x3f00;
        regs[1] = file;
        regs[2] = row.length;
        regs[3] = address & 15;
        regs[6] = address >> 4;
        Native.interrupt(0x21, regs);
    }

    public static void read(byte[] bmp) {
        if (null == bmp) {
            return;
        }
        if (file <= 0) {
            return;
        }
        byte[] row = new byte[width];
        for (int y = 0; y < height; y++) {
            read(row, y);
            int outOffset = y * width;
            for (int x = 0; x < width; x++) {
                if (outOffset + x < bmp.length) {
                    bmp[outOffset + x] = row[x];
                }
            }
        }
    }
}
