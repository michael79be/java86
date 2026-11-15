#include <stdint.h>
#include <stddef.h>

typedef struct {
    int file;
    long start;
    long end;
    uint32_t remaining;
    uint16_t constant_count;
} ClassFile;

int class_read(ClassFile *class_file, unsigned char *data, size_t length) {
    if ((uint32_t)length > class_file->remaining ||
        (int)length != sys_read(class_file->file, data, (uint16_t)length)) return 0;
    class_file->remaining -= (uint32_t)length;
    return 1;
}

uint16_t class_u16(const unsigned char *data) {
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

uint32_t class_u32(const unsigned char *data) {
    return ((uint32_t)class_u16(data) << 16) | class_u16(data + 2);
}

int class_seek(ClassFile *class_file, long offset) {
    if (offset < class_file->start || offset > class_file->end ||
        sys_seek(class_file->file, offset, 0) != offset) return 0;
    class_file->remaining = (uint32_t)(class_file->end - offset);
    return 1;
}

int class_open(ClassPath *class_path, const ClassLocation *location, ClassFile *class_file) {
    unsigned char header[10];

    class_file->file = class_path->files[location->archive_index];
    class_file->start = location->data_offset;
    class_file->end = location->data_offset + (long)location->data_size;
    class_file->remaining = location->data_size;
    if (sys_seek(class_file->file, location->data_offset, 0) != location->data_offset ||
        !class_read(class_file, header, sizeof(header)) ||
        0xca != header[0] || 0xfe != header[1] ||
        0xba != header[2] || 0xbe != header[3]) {
        sys_error("Invalid or truncated class header\r\n");
        return 0;
    }

    class_file->constant_count = class_u16(header + 8);
    if (0 == class_file->constant_count) {
        sys_error("Invalid constant pool count\r\n");
        return 0;
    }
    return 1;
}
