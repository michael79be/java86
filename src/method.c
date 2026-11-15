#include <stdint.h>
#include <string.h>

#define METHOD_CACHE_SIZE 10

typedef struct {
    uint16_t class_crc;
    uint16_t signature_crc;
    int archive_index;
    long code_offset;
    int valid;
} MethodLocation;

typedef struct {
    MethodLocation entries[METHOD_CACHE_SIZE];
    int next;
} MethodCache;

void method_cache_init(MethodCache *cache) {
    int i;

    for (i = 0; i < METHOD_CACHE_SIZE; i++) cache->entries[i].valid = 0;
    cache->next = 0;
}

uint16_t method_signature_crc(const char *signature) {
    uint16_t crc = 0;

    while (*signature) {
        crc = crc16_byte(crc, (unsigned char)*signature++);
    }
    return crc;
}

int method_utf8_matches(ClassFile *class_file, const ConstantPool *pool,
    uint16_t index, const char *text, size_t length) {
    unsigned char bytes[2];
    uint32_t offset;
    long saved;
    size_t i;
    int result = 1;

    if (0 == index || index >= pool->count || 1 != pool->entries[index].tag) return -1;
    offset = class_u32(pool->entries[index].data);
    saved = sys_seek(class_file->file, 0, 1);
    if (saved < 0 || offset > 0x7fffffffUL ||
        !class_seek(class_file, (long)offset) || !class_read(class_file, bytes, 2)) return -1;

    if (class_u16(bytes) != length) {
        result = 0;
    } else {
        for (i = 0; i < length; i++) {
            if (!class_read(class_file, bytes, 1)) {
                result = -1;
                break;
            }
            if (bytes[0] != (unsigned char)text[i]) {
                result = 0;
                break;
            }
        }
    }
    if (result >= 0 && !class_seek(class_file, saved)) return -1;
    return result;
}

int method_find(MethodCache *cache, ClassPath *class_path, ConstantPoolCache *pools,
    const char *class_name, const char *signature, MethodLocation **result) {
    const char *descriptor = strchr(signature, '(');
    size_t class_length = strlen(class_name);
    size_t name_length;
    size_t descriptor_length;
    uint16_t class_crc;
    uint16_t signature_crc;
    unsigned char header[8];
    uint16_t fields;
    uint16_t methods;
    uint16_t attributes;
    uint16_t flags;
    uint16_t i;
    uint16_t j;
    uint32_t attribute_length;
    long attribute_offset;
    long data_offset;
    int found;
    int matches;
    ClassLocation location;
    ConstantPool *pool;
    ClassFile class_file;
    MethodLocation *cached;

    if (0 == class_length || class_length > 65529UL ||
        NULL == descriptor || descriptor == signature) {
        sys_error("Invalid class name or method signature\r\n");
        return 0;
    }
    name_length = (size_t)(descriptor - signature);
    descriptor_length = strlen(descriptor);
    class_crc = class_name_crc(class_name, class_length);
    signature_crc = method_signature_crc(signature);
    for (i = 0; i < METHOD_CACHE_SIZE; i++) {
        cached = &cache->entries[i];
        if (cached->valid && cached->class_crc == class_crc &&
            cached->signature_crc == signature_crc) {
            *result = cached;
            return 1;
        }
    }

    found = class_path_find(class_path, class_name, &location);
    if (found <= 0) {
        if (0 == found) {
            sys_error("Class not found: ");
            sys_error(class_name);
            sys_error("\r\n");
        }
        return 0;
    }
    if (!constant_pool_load(pools, class_path, &location, &pool)) return 0;
    if (!class_open(class_path, &location, &class_file) ||
        !class_seek(&class_file, pool->end_offset)) goto invalid;

    if (!class_read(&class_file, header, 8)) goto invalid;
    if (0 != class_u16(header + 6)) {
        sys_error("Interfaces are not supported\r\n");
        return 0;
    }
    if (!class_read(&class_file, header, 2)) goto invalid;
    fields = class_u16(header);
    for (i = 0; i < fields; i++) {
        if (!class_read(&class_file, header, 8)) goto invalid;
        if (!(class_u16(header) & 0x0008)) {
            sys_error("Field is not static\r\n");
            return 0;
        }
        attributes = class_u16(header + 6);
        for (j = 0; j < attributes; j++) {
            if (!class_read(&class_file, header, 6)) goto invalid;
            attribute_length = class_u32(header + 2);
            data_offset = sys_seek(class_file.file, 0, 1);
            if (data_offset < 0 || attribute_length > class_file.remaining) goto invalid;
            if (!class_seek(&class_file, data_offset + (long)attribute_length)) goto invalid;
        }
    }
    if (!class_read(&class_file, header, 2)) goto invalid;
    methods = class_u16(header);

    for (i = 0; i < methods; i++) {
        if (!class_read(&class_file, header, 8)) goto invalid;
        flags = class_u16(header);
        attributes = class_u16(header + 6);
        matches = method_utf8_matches(&class_file, pool, class_u16(header + 2),
            signature, name_length);
        if (matches < 0) goto invalid;
        if (matches) {
            matches = method_utf8_matches(&class_file, pool, class_u16(header + 4),
                descriptor, descriptor_length);
            if (matches < 0) goto invalid;
        }
        if (matches && !(flags & 0x0008)) {
            sys_error("Method is not static: ");
            sys_error(signature);
            sys_error("\r\n");
            return 0;
        }

        for (j = 0; j < attributes; j++) {
            attribute_offset = sys_seek(class_file.file, 0, 1);
            if (attribute_offset < 0 || !class_read(&class_file, header, 6)) goto invalid;
            attribute_length = class_u32(header + 2);
            data_offset = sys_seek(class_file.file, 0, 1);
            if (data_offset < 0 || attribute_length > class_file.remaining) goto invalid;
            if (matches) {
                found = method_utf8_matches(&class_file, pool, class_u16(header), "Code", 4);
                if (found < 0) goto invalid;
                if (found) {
                    cached = &cache->entries[cache->next];
                    cached->class_crc = class_crc;
                    cached->signature_crc = signature_crc;
                    cached->archive_index = location.archive_index;
                    cached->code_offset = attribute_offset;
                    cached->valid = 1;
                    cache->next = (cache->next + 1) % METHOD_CACHE_SIZE;
                    *result = cached;
                    return 1;
                }
            }
            if (!class_seek(&class_file, data_offset + (long)attribute_length)) goto invalid;
        }
        if (matches) {
            sys_error("Method has no Code attribute: ");
            sys_error(signature);
            sys_error("\r\n");
            return 0;
        }
    }

    sys_error("Method not found: ");
    sys_error(signature);
    sys_error("\r\n");
    return 0;

invalid:
    sys_error("Invalid or truncated class methods\r\n");
    return 0;
}

typedef struct {
    uint16_t max_stack;
    uint16_t max_locals;
    uint32_t code_length;
    long code_start;
    ClassFile class_file;
} CodeAttribute;

int code_open(ClassPath *class_path, const ClassLocation *class_location,
    const MethodLocation *method, CodeAttribute *code) {
    unsigned char header[14];
    ClassFile *class_file = &code->class_file;

    class_file->file = class_path->files[method->archive_index];
    class_file->start = class_location->data_offset;
    class_file->end = class_location->data_offset + (long)class_location->data_size;
    if (!class_seek(class_file, method->code_offset) ||
        !class_read(class_file, header, sizeof(header))) {
        sys_error("Invalid or truncated Code attribute\r\n");
        return 0;
    }

    code->max_stack = class_u16(header + 6);
    code->max_locals = class_u16(header + 8);
    code->code_length = class_u32(header + 10);
    code->code_start = method->code_offset + (long)sizeof(header);
    if (0 == code->code_length || code->code_length > class_file->remaining) {
        sys_error("Invalid Code attribute length\r\n");
        return 0;
    }
    return 1;
}
