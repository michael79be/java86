package demo;

import driver.Mouse;
import platform.Native;
import ui.Graphics;

public class Desktop {
    public static void main(String[] args) {
        Graphics.create(0x101);
        for (int y = 0; y < Graphics.screenHeight; y++) {
            Graphics.putPixels(0, y, y, Graphics.screenWidth);
        }
        int mouse = Native.newProcess("driver/Mouse");
        int cursor = Native.newProcess("ui/Cursor");
        while (true) {
            if (2 == Mouse.button) {
                break;
            }
        }
        Native.killProcess(cursor);
        Graphics.destroy();
        Native.killProcess(mouse);
    }
}
