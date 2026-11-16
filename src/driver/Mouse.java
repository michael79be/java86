package driver;

import io.Console;
import platform.Native;
import ui.Graphics;

public class Mouse {
    public static int x;
    public static int y;
    public static int button;

    public static boolean click(int[] bounds) {
        return 1 == button
            && x >= bounds[0] && x < bounds[0] + bounds[2]
            && y >= bounds[1] && y < bounds[1] + bounds[3];
    }

    public static boolean down(int[] bounds, int button) {
        return (Mouse.button & button) != 0
            && x >= bounds[0] && x < bounds[0] + bounds[2]
            && y >= bounds[1] && y < bounds[1] + bounds[3];
    }

    public static boolean up(int[] bounds, int button) {
        return (Mouse.button & button) == 0
            && x >= bounds[0] && x < bounds[0] + bounds[2]
            && y >= bounds[1] && y < bounds[1] + bounds[3];
    }

    public static void main(String[] args) {
        int[] regs = new int[9];
        regs[0] = 0;
        Native.interrupt(0x33, regs);
        if (0 == regs[0]) {
            Console.println("Could not load mouse driver");
            return;
        }
        while (true) {
            regs[0] = 3;
            Native.interrupt(0x33, regs);
            x = regs[2];
            y = (regs[3] * Graphics.screenHeight) / 200;
            button = regs[1];
        }
    }
}
