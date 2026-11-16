package platform;

public class Native {
    public static native int newProcess(String className);

    public static native void killProcess(int id);

    public static native String toString(byte[] bytes);

    public static native byte[] getBytes(String s);

    public static native int readByte(int address);

    public static native void readBytes(int address, byte[] buffer, int windowWidth, int frameWidth, int offset);

    public static native void writeByte(int address, int value);

    public static native void writeBytes(int address, byte[] buffer, int windowWidth, int frameWidth, int offset, int ignoreColor);

    public static native void writeBytes(int address, int value, int count);

    public static native int readPort(int port);

    public static native int getAddress(byte[] buffer);

    public static native int[] interrupt(int number, int[] regs);
}
