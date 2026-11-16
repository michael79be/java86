package ui;

public class ToolBar {
    public static int[] create(int x, int y, int width, int height) {
        Graphics.fillRect(x, y, width, height, 7);
        Graphics.drawHLine(x, y, width, 15);
        Graphics.drawHLine(x, y + height - 1, width, 8);
        Graphics.drawVLine(x, y, height, 15);
        Graphics.drawVLine(x + width - 1, y, height, 8);
        return new int[]{x, y, width, height};
    }
}
