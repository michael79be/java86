package demo;

import driver.Mouse;
import platform.Native;
import ui.Button;
import ui.Font;
import ui.Graphics;
import ui.Menu;
import ui.MenuItem;
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
        boolean state = false;
        boolean menuShown = false;
        String maskPath = null;
        int[] quitItem = null;
        boolean quitPressed = false;
        int mouse = Native.newProcess("driver/Mouse");
        int cursor = Native.newProcess("ui/Cursor");
        while (true) {
            if (Mouse.down(start, 1) && !state) {
                Button.state(start, true);
                state = true;
            }
            if (Mouse.up(start, 1) && state) {
                state = false;
                if (!menuShown) {
                    Button.state(start, true);
                    maskPath = Menu.show(menu);
                    quitItem = MenuItem.create(menu, font, "Quit", 0);
                    menuShown = true;
                } else {
                    Button.state(start, false);
                    Menu.hide(menu, maskPath);
                    menuShown = false;
                    quitItem = null;
                    quitPressed = false;
                }
            }
            if (null != quitItem) {
                boolean buttonDown = (Mouse.button & 1) != 0;
                if (Mouse.in(quitItem) && buttonDown) {
                    if (!quitPressed) {
                        MenuItem.state(quitItem, true);
                        quitPressed = true;
                    }
                } else if (quitPressed) {
                    if (Mouse.in(quitItem)) {
                        break;
                    }
                    quitPressed = false;
                    MenuItem.state(quitItem, false);
                } else if (Mouse.in(quitItem)) {
                    MenuItem.state(quitItem, false);
                }
            }
        }
        Font.close(font);
        Native.killProcess(cursor);
        Graphics.destroy();
        Native.killProcess(mouse);
    }
}
