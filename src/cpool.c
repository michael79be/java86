#include <stdlib.h>
#include <stdint.h>

#define CONSTANT_POOL_CACHE_SIZE 10

typedef struct {
    uint8_t tag;
    uint8_t data[4];
} Constant;

typedef struct {
    uint16_t crc;
    uint16_t count;
    int archive_index;
    long end_offset;
    Constant *entries;
} ConstantPool;

typedef struct {
    ConstantPool pools[CONSTANT_POOL_CACHE_SIZE];
    int next;
} ConstantPoolCache;

void constant_pool_cache_init(ConstantPoolCache *cache) {
    int i;

    for (i = 0; i < CONSTANT_POOL_CACHE_SIZE; i++) {
        cache->pools[i].entries = NULL;
    }
    cache->next = 0;
}

void constant_pool_cache_destroy(ConstantPoolCache *cache) {
    int i;

    for (i = 0; i < CONSTANT_POOL_CACHE_SIZE; i++) {
        free(cache->pools[i].entries);
        cache->pools[i].entries = NULL;
    }
}

int constant_pool_load(ConstantPoolCache *cache, ClassPath *class_path,
    const ClassLocation *location, ConstantPool **result) {
    unsigned char length_bytes[2];
    uint32_t position;
    uint16_t count;
    uint16_t length;
    uint16_t i;
    int slot;
    ClassFile class_file;
    ConstantPool *pool;
    Constant *entry;
    size_t width;
    long offset;

    for (slot = 0; slot < CONSTANT_POOL_CACHE_SIZE; slot++) {
        pool = &cache->pools[slot];
        if (NULL != pool->entries && pool->crc == location->crc) {
            *result = pool;
            return 1;
        }
    }

    if (!class_open(class_path, location, &class_file)) return 0;
    count = class_file.constant_count;
    if ((uint32_t)count * sizeof(Constant) > (uint32_t)((size_t)-1)) {
        sys_error("Constant pool is too large\r\n");
        return 0;
    }

    pool = &cache->pools[cache->next];
    free(pool->entries);
    pool->entries = malloc((size_t)count * sizeof(Constant));
    if (NULL == pool->entries) {
        sys_error("Not enough memory for constant pool\r\n");
        return 0;
    }
    pool->entries[0].tag = 0;

    for (i = 1; i < count; i++) {
        entry = &pool->entries[i];
        if (!class_read(&class_file, &entry->tag, 1)) goto invalid;
        entry->data[0] = 0;
        entry->data[1] = 0;
        entry->data[2] = 0;
        entry->data[3] = 0;

        switch (entry->tag) {
        case 1:
            offset = sys_seek(class_file.file, 0, 1);
            if (offset < 0 || !class_read(&class_file, length_bytes, 2)) goto invalid;
            length = class_u16(length_bytes);
            if ((uint32_t)length > class_file.remaining ||
                sys_seek(class_file.file, (long)length, 1) < 0) goto invalid;
            class_file.remaining -= length;
            position = (uint32_t)offset;
            entry->data[0] = (uint8_t)(position >> 24);
            entry->data[1] = (uint8_t)(position >> 16);
            entry->data[2] = (uint8_t)(position >> 8);
            entry->data[3] = (uint8_t)position;
            break;
        case 7:
        case 8:
            if (!class_read(&class_file, entry->data + 2, 2)) goto invalid;
            break;
        case 3:
        case 4:
        case 9:
        case 10:
        case 11:
        case 12:
            width = sizeof(entry->data);
            if (!class_read(&class_file, entry->data, width)) goto invalid;
            break;
        default:
            sys_error("Unsupported constant pool tag ");
            sys_error_value(entry->tag);
            sys_error("\r\n");
            free(pool->entries);
            pool->entries = NULL;
            return 0;
        }
    }

    offset = sys_seek(class_file.file, 0, 1);
    if (offset < 0) goto invalid;
    pool->crc = location->crc;
    pool->count = count;
    pool->archive_index = location->archive_index;
    pool->end_offset = offset;
    cache->next = (cache->next + 1) % CONSTANT_POOL_CACHE_SIZE;
    *result = pool;
    return 1;

invalid:
    sys_error("Invalid or truncated constant pool\r\n");
    free(pool->entries);
    pool->entries = NULL;
    return 0;
}
