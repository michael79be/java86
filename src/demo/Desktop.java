package demo;

import io.Console;

public class Desktop {
    public static void main(String[] args) {
        ui.Graphics.create(0x13);
        ui.Graphics.drawPixel(0, 0, 15);
        Console.input();
        ui.Graphics.destroy();
    }
}
