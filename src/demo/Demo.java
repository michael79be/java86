package demo;

public class Demo {
    public static void main(String[] args) {
        platform.Native.newProcess("demo.Proc1");
        platform.Native.newProcess("demo.Proc2");
    }
}
