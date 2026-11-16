package driver;

import io.Console;
import platform.Native;

public class Mouse {
    public static int x;
    public static int y;
    public static int button;

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
            y = regs[3];
            button = regs[1];
        }
    }
}
