package demo;

import driver.Mouse;
import platform.Native;
import ui.Button;
import ui.Font;
import ui.Graphics;
import ui.Menu;
import ui.ToolBar;

public class Desktop {
    public static void main(String[] args) {
        Graphics.create(0x101);
        for (int y = 0; y < Graphics.screenHeight; y++) {
            Graphics.putPixels(0, y, y, Graphics.screenWidth);
        }
        ToolBar.create(0, Graphics.screenHeight - 30, Graphics.screenWidth, 30);
        int[] font = Font.open("HELVET12.BMP");
        int[] start = Button.create(4, Graphics.screenHeight - 28, 56, 24, font, "Start");
        int[] menu = Menu.create(4, Graphics.screenHeight - 130, 120, 100);
        Font.close(font);
        boolean state = false;
        boolean menuShown = false;
        String maskPath = null;
        int mouse = Native.newProcess("driver/Mouse");
        int cursor = Native.newProcess("ui/Cursor");
        while (true) {
            if (3 == Mouse.button) {
                break;
            }
            if (Mouse.down(start, 1) && !state) {
                Button.state(start, true);
                state = true;
            }
            if (Mouse.up(start, 1) && state) {
                state = false;
                if (!menuShown) {
                    Button.state(start, true);
                    maskPath = Menu.show(menu);
                    menuShown = true;
                } else {
                    Button.state(start, false);
                    Menu.hide(menu, maskPath);
                    menuShown = false;
                }
            }
        }
        Native.killProcess(cursor);
        Graphics.destroy();
        Native.killProcess(mouse);
    }
}
