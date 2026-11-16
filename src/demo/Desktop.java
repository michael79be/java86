package demo;

import driver.Mouse;
import platform.Native;

public class Desktop {
    public static void main(String[] args) {
        ui.Graphics.create(0x13);
        for (int y = 0; y < 200; y++) {
            ui.Graphics.putPixels(0, y, y, 320);
        }
        int mouse = Native.newProcess("driver/Mouse");
        int cursor = Native.newProcess("ui/Cursor");
        while (true) {
            if (0 != Mouse.button) {
                break;
            }
        }
        Native.killProcess(cursor);
        ui.Graphics.destroy();
        Native.killProcess(mouse);
    }
}
