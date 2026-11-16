package demo;

import io.Console;

public class Desktop {
    public static void main(String[] args) {
        ui.Graphics.create(0x13);
        for (int y = 0; y < 200; y++) {
            ui.Graphics.putPixels(0, y, y, 320);
        }
        Console.input();
        ui.Graphics.destroy();
    }
}
