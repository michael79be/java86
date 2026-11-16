package ui;

import io.Console;
import io.File;
import platform.Native;
import utils.StringUtils;

public class Font {
    public static int[] open(String path) {
        int file = File.open(path, 0x3d00);
        if (file < 0) {
            return null;
        }
        byte[] header = new byte[54];
        int headerAddress = Native.getAddress(header);
        int[] regs = new int[9];
        regs[0] = 0x3f00;
        regs[1] = file;
        regs[2] = 54;
        regs[3] = headerAddress & 0xF;
        regs[6] = headerAddress >> 4;
        Native.interrupt(0x21, regs);
        int offset = (header[10] & 0xff) | ((header[11] & 0xff) << 8) |
                ((header[12] & 0xff) << 16) | ((header[13] & 0xff) << 24);
        int width = (header[18] & 0xff) | ((header[19] & 0xff) << 8);
        int height = (header[22] & 0xff) | ((header[23] & 0xff) << 8);

        int stride = (width + 3) & ~3;
        int cellHeight = height / 223;
        int[] result = new int[229];
        result[0] = file;
        result[1] = width;
        result[2] = height;
        result[3] = offset;
        result[4] = stride;
        result[5] = cellHeight;
        loadWidths(path, result);
        return result;
    }

    public static void loadWidths(String path, int[] result) {
        String name = baseName(path);
        int wFile = File.open("FONTS.DAT", 0x3d00);
        if (wFile < 0) {
            return;
        }
        int size = File.seek(wFile, 0, 2);
        if (size < 0) {
            File.close(wFile);
            return;
        }
        File.seek(wFile, 0, 0);
        byte[] data = File.read(wFile, size);
        File.close(wFile);
        if (data == null || data.length == 0) {
            return;
        }
        int p = 0;
        while (p < data.length) {
            int lineEnd = p;
            while (lineEnd < data.length && data[lineEnd] != 13 && data[lineEnd] != 10) {
                lineEnd++;
            }
            int lineLen = lineEnd - p;
            if (lineLen > 0 && lineLen >= StringUtils.length(name) + 223) {
                boolean match = true;
                for (int i = 0; i < StringUtils.length(name); i++) {
                    if ((data[p + i] & 0xff) != StringUtils.charAt(name, i)) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    int widthStart = lineEnd - 223;
                    for (int i = 0; i < 223; i++) {
                        result[6 + i] = (data[widthStart + i] & 0xff) - '0';
                    }
                    return;
                }
            }
            p = lineEnd;
            while (p < data.length && (data[p] == 13 || data[p] == 10)) {
                p++;
            }
        }
    }

    public static String baseName(String path) {
        int last = StringUtils.lastIndexOf(path, '/');
        if (last < 0) {
            last = StringUtils.lastIndexOf(path, '\\');
        }
        if (last < 0) {
            return path;
        }
        return StringUtils.substring(path, last + 1);
    }

    public static void drawChar(int[] font, int x, int y, int charCode) {
        if (null == font || charCode < 33 || charCode > 255) {
            return;
        }
        int file = font[0];
        int width = font[1];
        int height = font[2];
        int offset = font[3];
        int stride = font[4];
        int cellHeight = font[5];
        int cw = font[6 + charCode - 33];
        if (cw <= 0 || cw > width) {
            Console.println("Missing char width");
            cw = width;
        }
        int band = charCode - 33;
        int topRow = height - 1 - band * cellHeight;
        byte[] row = new byte[cw];
        int address = Native.getAddress(row);
        int[] regs = new int[9];

        for (int r = 0; r < cellHeight; r++) {
            int fileRow = topRow - r;
            if (fileRow < 0) {
                break;
            }
            int fileOffset = offset + fileRow * stride;
            regs[0] = 0x4200;
            regs[1] = file;
            regs[2] = fileOffset >> 16;
            regs[3] = fileOffset & 0xffff;
            Native.interrupt(0x21, regs);

            regs[0] = 0x3f00;
            regs[1] = file;
            regs[2] = cw;
            regs[3] = address & 0xF;
            regs[6] = address >> 4;
            Native.interrupt(0x21, regs);

            int screenAddress = 0xa0000 + (y + r) * Graphics.screenWidth + x;
            Native.writeBytes(screenAddress, row, Graphics.screenWidth, cw, 0, 15);
        }
    }

    public static int charWidth(int[] font, int charCode) {
        if (null == font) {
            return 0;
        }
        if (charCode == 32) {
            int w = font[6 + 'i' - 33];
            return w > 0 ? w : (font[5] / 3);
        }
        if (charCode >= 33 && charCode <= 255) {
            return font[6 + charCode - 33];
        }
        return 0;
    }

    public static int stringWidth(int[] font, String s) {
        if (null == font || null == s) {
            return 0;
        }
        int total = 0;
        int len = StringUtils.length(s);
        for (int i = 0; i < len; i++) {
            total += charWidth(font, StringUtils.charAt(s, i));
        }
        return total;
    }

    public static void drawString(int[] font, int x, int y, String s) {
        if (null == font || null == s) {
            return;
        }
        int len = StringUtils.length(s);
        if (len == 0) {
            return;
        }
        int[] widths = new int[len];
        int totalWidth = 0;
        for (int i = 0; i < len; i++) {
            int cc = StringUtils.charAt(s, i);
            int w = charWidth(font, cc);
            widths[i] = w;
            totalWidth += w;
        }
        if (totalWidth <= 0) {
            return;
        }

        int file = font[0];
        int height = font[2];
        int offset = font[3];
        int stride = font[4];
        int cellHeight = font[5];

        byte[] buffer = new byte[totalWidth * cellHeight];
        for (int i = 0; i < buffer.length; i++) {
            buffer[i] = 15;
        }

        int blockSize = cellHeight * stride;
        byte[] charData = new byte[blockSize];
        int charAddress = Native.getAddress(charData);
        int[] regs = new int[9];

        int bx = 0;
        for (int i = 0; i < len; i++) {
            int cw = widths[i];
            if (cw <= 0) {
                continue;
            }
            int cc = StringUtils.charAt(s, i);
            if (cc >= 33 && cc <= 255) {
                int band = cc - 33;
                int bottomRow = height - (band + 1) * cellHeight;
                if (bottomRow >= 0) {
                    int fileOffset = offset + bottomRow * stride;

                    regs[0] = 0x4200;
                    regs[1] = file;
                    regs[2] = fileOffset >> 16;
                    regs[3] = fileOffset & 0xffff;
                    Native.interrupt(0x21, regs);

                    regs[0] = 0x3f00;
                    regs[1] = file;
                    regs[2] = blockSize;
                    regs[3] = charAddress & 0xF;
                    regs[6] = charAddress >> 4;
                    Native.interrupt(0x21, regs);

                    for (int r = 0; r < cellHeight; r++) {
                        int srcRow = (cellHeight - 1 - r) * stride;
                        int dstRow = r * totalWidth + bx;
                        for (int c = 0; c < cw; c++) {
                            buffer[dstRow + c] = charData[srcRow + c];
                        }
                    }
                }
            }
            bx += cw;
        }

        int screenAddress = 0xa0000 + y * Graphics.screenWidth + x;
        Native.writeBytes(screenAddress, buffer, Graphics.screenWidth, totalWidth, 0, 15);
    }

    public static void close(int[] font) {
        File.close(font[0]);
    }
}