package ui;

public class MenuItem {
    public static int[] create(int[] menu, int[] font, String text, int pos) {
        int x = menu[0];
        int y = menu[1];
        int width = menu[2];
        int itemHeight = font[5] + 14;
        int iy = y + pos * itemHeight;
        int[] item = new int[]{x + 5, iy + 4, width - 10, itemHeight - 8, -1};
        Cursor.setHidden(true);
        Graphics.fillRect(item[0], item[1], item[2], item[3], 7);
        if (null != text && null != font) {
            int tx = item[0] + 5;
            int ty = item[1] + (item[3] - font[5]) / 2;
            Font.drawString(font, tx, ty, text);
        }
        Cursor.setHidden(false);
        return item;
    }

    public static void state(int[] item, boolean pressed) {
        int x = item[0];
        int y = item[1];
        int width = item[2];
        int height = item[3];
        if (item[4] == (pressed ? 1 : 0)) {
            return;
        }
        item[4] = pressed ? 1 : 0;
        int topLeft = pressed ? 8 : 15;
        int bottomRight = pressed ? 15 : 8;
        Cursor.setHidden(true);
        Graphics.drawHLine(x, y, width, topLeft);
        Graphics.drawHLine(x, y + height - 1, width, bottomRight);
        Graphics.drawVLine(x, y, height, topLeft);
        Graphics.drawVLine(x + width - 1, y, height, bottomRight);
        Cursor.setHidden(false);
    }
}
