package ui;

import utils.StringUtils;

public class Menu {
    public static int[] create(int x, int y, int width, int height) {
        return new int[]{x, y, width, height};
    }

    public static void draw(int[] menu) {
        Graphics.fillRect(menu[0], menu[1], menu[2], menu[3], 7);
        Graphics.drawHLine(menu[0], menu[1], menu[2], 15);
        Graphics.drawHLine(menu[0], menu[1] + menu[3] - 1, menu[2], 8);
        Graphics.drawVLine(menu[0], menu[1], menu[3], 15);
        Graphics.drawVLine(menu[0] + menu[2] - 1, menu[1], menu[3], 8);
    }

    public static String show(int[] menu) {
        int x = menu[0];
        int y = menu[1];
        int width = menu[2];
        int height = menu[3];
        String path = StringUtils.concat(StringUtils.concat("M", StringUtils.toString(x)), StringUtils.concat(StringUtils.toString(y), ".MSK"));
        Cursor.setHidden(true);
        Graphics.getPixels(x, y, width, height, path);
        draw(menu);
        Cursor.setHidden(false);
        return path;
    }

    public static void hide(int[] menu, String path) {
        Cursor.setHidden(true);
        Graphics.putPixels(menu[0], menu[1], menu[2], menu[3], path);
        Cursor.setHidden(false);
    }
}
