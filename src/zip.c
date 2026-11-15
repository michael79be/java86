#include <stdint.h>
#include <stddef.h>

uint16_t zip_u16(const unsigned char *data) {
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

uint32_t zip_u32(const unsigned char *data) {
    return (uint32_t)zip_u16(data) | ((uint32_t)zip_u16(data + 2) << 16);
}

int zip_read(int file, long offset, void *data, uint16_t size) {
    return sys_seek(file, offset, 0) == offset &&
        sys_read(file, data, size) == (int)size;
}

int zip_range(long offset, uint32_t size, long limit) {
    return offset >= 0 && offset <= limit && size <= (uint32_t)(limit - offset);
}

int class_name_char(const char *name, size_t length, size_t index) {
    if (index < length) {
        return '.' == name[index] ? '/' : (unsigned char)name[index];
    }
    return (unsigned char)".class"[index - length];
}

int class_name_matches(int file, long offset, uint16_t entry_length,
    const char *name, size_t length) {
    size_t i;
    unsigned char character;

    if ((size_t)entry_length != length + 6) return 0;
    if (sys_seek(file, offset, 0) != offset) return -1;

    for (i = 0; i < length + 6; i++) {
        if (!sys_read_byte(file, &character)) return -1;
        if (character != class_name_char(name, length, i)) return 0;
    }
    return 1;
}

int zip_local_entry(int file, long file_size, long offset,
    const char *name, size_t name_length, long *location, uint32_t *entry_size,
    int from_central, uint16_t central_flags, uint16_t central_method,
    uint32_t central_size, uint32_t central_uncompressed_size, long *next_offset) {
    unsigned char header[30];
    uint16_t flags;
    uint16_t method;
    uint16_t entry_length;
    uint16_t extra_length;
    uint32_t size;
    uint32_t uncompressed_size;
    long name_offset;
    long data_offset;
    int matches;

    if (!zip_range(offset, (uint32_t)sizeof(header), file_size) ||
        !zip_read(file, offset, header, (uint16_t)sizeof(header)) ||
        0x04034b50UL != zip_u32(header)) return -1;

    flags = zip_u16(header + 6);
    method = zip_u16(header + 8);
    size = zip_u32(header + 18);
    uncompressed_size = zip_u32(header + 22);
    entry_length = zip_u16(header + 26);
    extra_length = zip_u16(header + 28);
    name_offset = offset + (long)sizeof(header);
    if (!zip_range(name_offset, (uint32_t)entry_length + extra_length, file_size)) return -1;
    data_offset = name_offset + (long)entry_length + (long)extra_length;

    if (from_central) {
        if (flags != central_flags || method != central_method) return -1;
        size = central_size;
        uncompressed_size = central_uncompressed_size;
    } else if (flags & 8) {
        sys_error("ZIP data descriptor requires a central directory\r\n");
        return -1;
    }
    if (!zip_range(data_offset, size, file_size)) return -1;

    matches = class_name_matches(file, name_offset, entry_length, name, name_length);
    if (matches < 0) return -1;
    if (matches) {
        if (0 != method || (flags & 1) || size != uncompressed_size) {
            sys_error("Class entry must be uncompressed and unencrypted\r\n");
            return -1;
        }
        *location = data_offset;
        *entry_size = size;
    }
    if (NULL != next_offset) *next_offset = data_offset + (long)size;
    return matches;
}

int zip_find_class(int file, const char *name, size_t name_length,
    long *location, uint32_t *entry_size) {
    unsigned char header[46];
    long file_size;
    long eocd;
    long search_start;
    long directory_offset;
    long directory_end;
    long offset;
    long next_offset;
    uint32_t directory_size;
    uint32_t local_offset;
    uint32_t record_size;
    uint16_t entries;
    uint16_t i;
    int result;
    int found_eocd = 0;

    file_size = sys_seek(file, 0, 2);
    if (file_size < 0 || file_size > 0x7fffffffL) return -1;
    search_start = file_size > 65557L ? file_size - 65557L : 0;
    for (eocd = file_size - 22; eocd >= search_start; eocd--) {
        if (!zip_read(file, eocd, header, 22)) return -1;
        if (0x06054b50UL == zip_u32(header) &&
            zip_u16(header + 20) == (uint32_t)(file_size - eocd - 22)) {
            found_eocd = 1;
            break;
        }
    }

    if (found_eocd) {
        if (0 != zip_u16(header + 4) || 0 != zip_u16(header + 6) ||
            zip_u16(header + 8) != zip_u16(header + 10) ||
            0xffff == zip_u16(header + 10)) return -1;
        entries = zip_u16(header + 10);
        directory_size = zip_u32(header + 12);
        local_offset = zip_u32(header + 16);
        if (local_offset > (uint32_t)file_size) return -1;
        directory_offset = (long)local_offset;
        if (!zip_range(directory_offset, directory_size, eocd)) return -1;
        directory_end = directory_offset + (long)directory_size;
        offset = directory_offset;

        for (i = 0; i < entries; i++) {
            if (!zip_range(offset, (uint32_t)sizeof(header), directory_end) ||
                !zip_read(file, offset, header, (uint16_t)sizeof(header)) ||
                0x02014b50UL != zip_u32(header)) return -1;
            record_size = (uint32_t)sizeof(header) + zip_u16(header + 28) +
                zip_u16(header + 30) + zip_u16(header + 32);
            if (!zip_range(offset, record_size, directory_end) ||
                0 != zip_u16(header + 34)) return -1;
            result = class_name_matches(file, offset + (long)sizeof(header),
                zip_u16(header + 28), name, name_length);
            if (result < 0) return -1;
            if (result) {
                local_offset = zip_u32(header + 42);
                if (local_offset > (uint32_t)file_size) return -1;
                result = zip_local_entry(file, file_size, (long)local_offset,
                    name, name_length, location, entry_size, 1, zip_u16(header + 8),
                    zip_u16(header + 10), zip_u32(header + 20), zip_u32(header + 24), NULL);
                return 1 == result ? 1 : -1;
            }
            offset += (long)record_size;
        }
        return 0;
    }

    offset = 0;
    while (offset < file_size) {
        result = zip_local_entry(file, file_size, offset, name, name_length,
            location, entry_size, 0, 0, 0, 0, 0, &next_offset);
        if (0 != result) return result;
        offset = next_offset;
    }
    return 0;
}
