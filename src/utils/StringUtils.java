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
}
