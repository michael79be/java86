#include <stdint.h>
#include <string.h>

#ifdef __ia16__
int sys_open(const char *path) {
    uint16_t handle;
    uint16_t failed;

    __asm__ volatile (
        "movw $0x3d00, %%ax\n\t"
        "int $0x21\n\t"
        "sbbw %%bx, %%bx"
        : "=a" (handle), "=b" (failed)
        : "d" (path)
        : "cc", "memory");
    return failed ? -1 : (int)handle;
}

void sys_close(int file) {
    __asm__ volatile (
        "movb $0x3e, %%ah\n\t"
        "int $0x21"
        :
        : "b" (file)
        : "ax", "cc", "memory");
}

int sys_read(int file, void *data, uint16_t length) {
    uint16_t count;
    uint16_t failed;

    __asm__ volatile (
        "movb $0x3f, %%ah\n\t"
        "int $0x21\n\t"
        "sbbw %%si, %%si"
        : "=a" (count), "=S" (failed)
        : "b" (file), "c" (length), "d" (data)
        : "cc", "memory");
    return failed ? -1 : (int)count;
}

void sys_write(int file, const void *data, uint16_t length) {
    __asm__ volatile (
        "movb $0x40, %%ah\n\t"
        "int $0x21"
        :
        : "b" (file), "c" (length), "d" (data)
        : "ax", "cc", "memory");
}

long sys_seek(int file, long offset, int origin) {
    uint16_t low = (uint16_t)offset;
    uint16_t high = (uint16_t)((uint32_t)offset >> 16);
    uint16_t failed;

    __asm__ volatile (
        "movb $0x42, %%ah\n\t"
        "int $0x21\n\t"
        "sbbw %%si, %%si"
        : "=a" (low), "=d" (high), "=S" (failed)
        : "0" ((uint16_t)origin), "b" (file), "c" (high), "1" (low)
        : "cc", "memory");
    if (failed) return -1;
    return (long)(((uint32_t)high << 16) | low);
}
#else
#include <fcntl.h>
#include <unistd.h>

int sys_open(const char *path) {
    return open(path, O_RDONLY);
}

void sys_close(int file) {
    close(file);
}

int sys_read(int file, void *data, uint16_t length) {
    ssize_t count = read(file, data, length);
    return count < 0 ? -1 : (int)count;
}

void sys_write(int file, const void *data, uint16_t length) {
    ssize_t ignored = write(file, data, length);
    (void)ignored;
}

long sys_seek(int file, long offset, int origin) {
    return (long)lseek(file, offset, origin);
}
#endif

int sys_read_byte(int file, unsigned char *byte) {
    return 1 == sys_read(file, byte, 1);
}

void sys_print(const char *text) {
    sys_write(1, text, (uint16_t)strlen(text));
}

void sys_error(const char *text) {
    sys_write(2, text, (uint16_t)strlen(text));
}

void sys_error_value(uint32_t value) {
    char text[11];
    int i = 10;

    text[10] = '\0';
    do {
        text[--i] = (char)('0' + (value % 10));
        value /= 10;
    } while (0 != value && i > 0);
    sys_error(&text[i]);
}
