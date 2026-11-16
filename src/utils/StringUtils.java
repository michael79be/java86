package utils;

import platform.Native;

public class StringUtils {
    public static boolean isEmpty(String str) {
        byte[] bytes = Native.getBytes(str);
        return bytes.length == 0;
    }

    public static boolean equals(String str1, String str2) {
        byte[] bytes1 = Native.getBytes(str1);
        byte[] bytes2 = Native.getBytes(str2);
        if (bytes1.length != bytes2.length) {
            return false;
        }
        for (int i = 0; i < bytes1.length; i++) {
            if (bytes1[i] != bytes2[i]) {
                return false;
            }
        }
        return true;
    }

    public static boolean startsWith(String str, String prefix) {
        byte[] bytes = Native.getBytes(str);
        byte[] prefixBytes = Native.getBytes(prefix);
        if (prefixBytes.length > bytes.length) {
            return false;
        }
        for (int i = 0; i < prefixBytes.length; i++) {
            if (bytes[i] != prefixBytes[i]) {
                return false;
            }
        }
        return true;
    }

    public static String substring(String str, int begin) {
        byte[] bytes = Native.getBytes(str);
        byte[] result = new byte[bytes.length - begin];
        for (int i = 0; i < result.length; i++) {
            result[i] = bytes[begin + i];
        }
        return Native.toString(result);
    }

    public static int lastIndexOf(String str, int ch) {
        byte[] bytes = Native.getBytes(str);
        for (int i = bytes.length - 1; i >= 0; i--) {
            if ((bytes[i] & 0xff) == ch) {
                return i;
            }
        }
        return -1;
    }

    public static int length(String str) {
        return Native.getBytes(str).length;
    }

    public static int charAt(String str, int i) {
        byte[] bytes = Native.getBytes(str);
        return bytes[i] & 0xff;
    }

    public static String toString(int value) {
        if (value == 0) {
            return "0";
        }
        boolean negative = value < 0;
        if (negative) {
            value = -value;
        }
        int digits = 0;
        int temp = value;
        while (temp > 0) {
            digits++;
            temp /= 10;
        }
        byte[] result = new byte[negative ? digits + 1 : digits];
        int pos = digits;
        if (negative) {
            result[0] = 45;
        }
        while (value > 0) {
            result[--pos] = (byte) (48 + value % 10);
            value /= 10;
        }
        return Native.toString(result);
    }

    public static String concat(String s1, String s2) {
        int total = length(s1) + length(s2);
        byte[] result = new byte[total];
        int pos = 0;
        if (s1 != null) {
            byte[] bytes = Native.getBytes(s1);
            for (int i = 0; i < bytes.length; i++) {
                result[pos++] = bytes[i];
            }
        }
        if (s2 != null) {
            byte[] bytes = Native.getBytes(s2);
            for (int i = 0; i < bytes.length; i++) {
                result[pos++] = bytes[i];
            }
        }
        return Native.toString(result);
    }
}
