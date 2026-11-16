package ui;

public class Button {
    public static int[] create(int x, int y, int width, int height, int[] font, String text) {
        int[] bounds = new int[]{x, y, width, height};
        Cursor.setHidden(true);
        Graphics.fillRect(x, y, width, height, 7);
        state(bounds, false);
        if (null != text && null != font) {
            int tw = Font.stringWidth(font, text);
            int tx = x + (width - tw) / 2;
            int ty = y + (height - font[5]) / 2;
            Font.drawString(font, tx, ty, text);
        }
        Cursor.setHidden(false);
        return bounds;
    }

    public static void state(int[] bounds, boolean pressed) {
        int x = bounds[0];
        int y = bounds[1];
        int width = bounds[2];
        int height = bounds[3];
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
