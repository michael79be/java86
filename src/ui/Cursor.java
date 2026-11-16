package ui;

import driver.Mouse;
import platform.Native;

public class Cursor {
    public static int x;
    public static int y;
    public static byte[] cursor;
    public static byte[] mask;
    public static int cursorWidth;
    public static int cursorHeight;
    public static boolean hide;

    public static void main(String[] args) {
        hide = false;

        if (BMP.open("CURSOR.BMP") > 0) {
            cursorWidth = BMP.width;
            cursorHeight = BMP.height;
            cursor = new byte[cursorWidth * cursorHeight];
            BMP.read(cursor);
            BMP.close();
        }

        x = Mouse.x;
        y = Mouse.y;
        mask = new byte[cursorWidth * cursorHeight];
        getMask();

        while (true) {
            if (hide) {
                continue;
            }
            if (x != Mouse.x || y != Mouse.y) {
                Graphics.waitVbl();
                drawMask(x, y);
                x = Mouse.x;
                y = Mouse.y;
                getMask();
                drawCursor();
            }
        }
    }

    public static void drawCursor() {
        int address = 0xa0000 + y * Graphics.screenWidth;
        Native.writeBytes(address, cursor, Graphics.screenWidth, cursorWidth, x, 13);
    }

    public static void getMask() {
        int address = 0xa0000 + y * Graphics.screenWidth;
        Native.readBytes(address, mask, Graphics.screenWidth, cursorWidth, x);
    }

    public static void drawMask(int x, int y) {
        int address = 0xa0000 + y * Graphics.screenWidth;
        Native.writeBytes(address, mask, Graphics.screenWidth, cursorWidth, x, -1);
    }

    public static void setHidden(boolean hidden) {
        if (null != mask && hidden) {
            drawMask(x, y);
        }
        if (null != cursor) {
            drawCursor();
        }
        hide = hidden;
    }
}
