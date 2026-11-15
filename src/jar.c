#include <stdint.h>
#include <string.h>

#include "zip.c"

#define CLASS_CACHE_SIZE 10
#define MAX_CLASSPATH 10

typedef struct {
    uint16_t crc;
    int archive_index;
    long data_offset;
    uint32_t data_size;
} ClassLocation;

typedef struct {
    int files[MAX_CLASSPATH];
    int file_count;
    ClassLocation cache[CLASS_CACHE_SIZE];
    int cache_count;
    int cache_next;
} ClassPath;

uint16_t crc16_byte(uint16_t crc, unsigned char byte) {
    int bit;

    crc ^= byte;
    for (bit = 0; bit < 8; bit++) {
        crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}

uint16_t class_name_crc(const char *name, size_t length) {
    uint16_t crc = 0;
    size_t i;

    for (i = 0; i < length + 6; i++) {
        crc = crc16_byte(crc, (unsigned char)class_name_char(name, length, i));
    }
    return crc;
}

void class_path_init(ClassPath *class_path) {
    class_path->file_count = 0;
    class_path->cache_count = 0;
    class_path->cache_next = 0;
}

int class_path_open(ClassPath *class_path, char *paths) {
    char *path = strtok(paths, ";");

    if (NULL == path) {
        sys_error("Empty classpath\r\n");
        return 0;
    }

    while (NULL != path) {
        int archive;

        if (class_path->file_count == MAX_CLASSPATH) {
            sys_error("Too many classpath files\r\n");
            return 0;
        }

        archive = sys_open(path);
        if (archive < 0) {
            sys_error(path);
            sys_error(": cannot open classpath file\r\n");
            return 0;
        }

        class_path->files[class_path->file_count++] = archive;
        sys_print("Classpath: ");
        sys_print(path);
        sys_print("\r\n");
        path = strtok(NULL, ";");
    }

    return 1;
}

int class_path_find(ClassPath *class_path, const char *name, ClassLocation *location) {
    size_t length = strlen(name);
    uint16_t crc;
    int i;
    int result;

    if (0 == length || length > 65529UL) {
        sys_error("Invalid class name\r\n");
        return -1;
    }
    crc = class_name_crc(name, length);
    for (i = 0; i < class_path->cache_count; i++) {
        ClassLocation *cached = &class_path->cache[i];
        if (cached->crc != crc) continue;
        *location = *cached;
        return 1;
    }

    for (i = 0; i < class_path->file_count; i++) {
        result = zip_find_class(class_path->files[i], name, length,
            &location->data_offset, &location->data_size);
        if (result < 0) {
            sys_error("Invalid or unsupported ZIP in classpath\r\n");
            return -1;
        }
        if (result) {
            location->crc = crc;
            location->archive_index = i;
            class_path->cache[class_path->cache_next] = *location;
            class_path->cache_next = (class_path->cache_next + 1) % CLASS_CACHE_SIZE;
            if (class_path->cache_count < CLASS_CACHE_SIZE) class_path->cache_count++;
            return 1;
        }
    }
    return 0;
}

void class_path_close(ClassPath *class_path) {
    int i;

    for (i = 0; i < class_path->file_count; i++) {
        sys_close(class_path->files[i]);
    }
}
