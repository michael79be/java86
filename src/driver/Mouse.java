package driver;

import platform.Native;

public class Mouse {
    public static int x;
    public static int y;
    public static int button;

    public static void main(String[] args) {
        int[] regs = new int[9];
        while (true) {
            regs[0] = 3;
            Native.interrupt(0x33, regs);
            x = regs[2];
            y = regs[3];
            button = regs[1];
        }
    }
}
