#include <stdint.h>
#include <string.h>

#define FRAME_STACK_SIZE 16
#define FRAME_LOCALS_SIZE 8
#define MAX_UTF8_NAME 128

typedef struct ProcessTable ProcessTable;

typedef struct {
    ProcessTable *processes;
    ClassPath *class_path;
    ConstantPoolCache *pools;
    MethodCache *methods;
    ConstantPool *pool;
    ClassLocation location;
    CodeAttribute code;
    uint32_t pc;
    uint32_t stack[FRAME_STACK_SIZE];
    int sp;
    uint32_t locals[FRAME_LOCALS_SIZE];
} Frame;

typedef struct {
    Frame frame;
    int index;
    int parent;
    int active;
    int blocked;
} Process;

int process_spawn(ProcessTable *table, const char *class_name);
int process_call(ProcessTable *table, const char *class_name, const char *signature,
    const uint32_t *args, int arg_count, int parent);
int process_return(ProcessTable *table, Process *process, int has_value, uint32_t value);
int process_kill(ProcessTable *table, int id);
ArrayTable *process_arrays(ProcessTable *table);
FieldTable *process_fields(ProcessTable *table);
int process_gc(ProcessTable *table);
int native_invoke(Frame *frame, const char *signature, const uint32_t *args);
int opcode_math(Frame *frame, unsigned char opcode);

int frame_fetch(Frame *frame, unsigned char *data, uint32_t length) {
    if (frame->code.code_length - frame->pc < length ||
        !class_read(&frame->code.class_file, data, length)) {
        sys_error("Truncated bytecode\r\n");
        return 0;
    }
    frame->pc += length;
    return 1;
}

int frame_push(Frame *frame, uint32_t value) {
    if (frame->sp >= (int)frame->code.max_stack || frame->sp >= FRAME_STACK_SIZE) {
        sys_error("Operand stack overflow\r\n");
        return 0;
    }
    frame->stack[frame->sp++] = value;
    return 1;
}

int frame_pop(Frame *frame, uint32_t *value) {
    if (frame->sp <= 0) {
        sys_error("Operand stack underflow\r\n");
        return 0;
    }
    *value = frame->stack[--frame->sp];
    return 1;
}

const Constant *frame_constant(Frame *frame, uint16_t index, uint8_t tag) {
    if (0 == index || index >= frame->pool->count ||
        tag != frame->pool->entries[index].tag) {
        sys_error("Invalid constant pool index ");
        sys_error_value(index);
        sys_error("\r\n");
        return NULL;
    }
    return &frame->pool->entries[index];
}

int utf8_read_at(ClassFile *class_file, uint32_t offset, char *buffer, size_t capacity) {
    unsigned char bytes[2];
    uint16_t length;
    long saved;

    saved = sys_seek(class_file->file, 0, 1);
    if (saved < 0 || offset > 0x7fffffffUL || !class_seek(class_file, (long)offset) ||
        !class_read(class_file, bytes, 2)) return 0;
    length = class_u16(bytes);
    if ((size_t)length + 1 > capacity) return 0;
    if (!class_read(class_file, (unsigned char *)buffer, length) ||
        !class_seek(class_file, saved)) return 0;
    buffer[length] = 0;
    return 1;
}

int frame_utf8(Frame *frame, uint16_t index, char *buffer, size_t capacity) {
    const Constant *entry = frame_constant(frame, index, 1);

    if (NULL == entry) return 0;
    if (!utf8_read_at(&frame->code.class_file, class_u32(entry->data),
        buffer, capacity)) {
        sys_error("Invalid UTF-8 constant\r\n");
        return 0;
    }
    return 1;
}

#define STRING_OFFSET_MASK 0x00ffffffUL

int string_open(ClassPath *class_path, uint32_t reference, int *file, uint16_t *length) {
    int archive = (int)((reference >> 24) & 0x7f);
    unsigned char bytes[2];
    long offset = (long)(reference & STRING_OFFSET_MASK);

    if ((reference & ARRAY_REF_FLAG) || archive >= class_path->file_count) return 0;
    *file = class_path->files[archive];
    if (sys_seek(*file, offset, 0) != offset ||
        2 != sys_read(*file, bytes, 2)) return 0;
    *length = class_u16(bytes);
    return 1;
}

int string_read(ClassPath *class_path, uint32_t reference, char *buffer, size_t capacity) {
    int file;
    uint16_t length;

    if (!string_open(class_path, reference, &file, &length)) return 0;
    if ((size_t)length + 1 > capacity) return 0;
    if ((int)length != sys_read(file, buffer, length)) return 0;
    buffer[length] = 0;
    return 1;
}

int descriptor_arg_slots(const char *descriptor) {
    const char *character = descriptor + 1;
    int slots = 0;

    while ('\0' != *character && ')' != *character) {
        while ('[' == *character) character++;
        if ('L' == *character) {
            while ('\0' != *character && ';' != *character) character++;
            if ('\0' == *character) return -1;
        } else if (NULL == strchr("BCFISZ", *character)) {
            return -1;
        }
        character++;
        slots++;
    }
    return ')' == *character ? slots : -1;
}

int opcode_ldc(Frame *frame) {
    unsigned char index;
    const Constant *entry;
    const Constant *utf8;

    if (!frame_fetch(frame, &index, 1)) return 0;
    if (0 == index || index >= frame->pool->count) {
        sys_error("Invalid constant pool index ");
        sys_error_value(index);
        sys_error("\r\n");
        return 0;
    }
    entry = &frame->pool->entries[index];
    switch (entry->tag) {
    case 3:
        return frame_push(frame, class_u32(entry->data));
    case 8: {
        uint32_t offset;
        utf8 = frame_constant(frame, class_u16(entry->data + 2), 1);
        if (NULL == utf8) return 0;
        offset = class_u32(utf8->data);
        if (offset > STRING_OFFSET_MASK || frame->pool->archive_index > 0x7e) {
            sys_error("String constant out of range\r\n");
            return 0;
        }
        return frame_push(frame,
            offset | ((uint32_t)frame->pool->archive_index << 24));
    }
    default:
        sys_error("Unsupported ldc constant tag ");
        sys_error_value(entry->tag);
        sys_error("\r\n");
        return 0;
    }
}

int frame_local_valid(Frame *frame, int index) {
    if (index < (int)frame->code.max_locals && index < FRAME_LOCALS_SIZE) return 1;
    sys_error("Invalid local index ");
    sys_error_value((uint32_t)index);
    sys_error("\r\n");
    return 0;
}

int opcode_load(Frame *frame, int index) {
    if (!frame_local_valid(frame, index)) return 0;
    return frame_push(frame, frame->locals[index]);
}

int opcode_store(Frame *frame, int index) {
    if (!frame_local_valid(frame, index)) return 0;
    return frame_pop(frame, &frame->locals[index]);
}

int opcode_branch(Frame *frame, uint32_t opcode_pc, int condition) {
    unsigned char offset[2];
    long target;

    if (!frame_fetch(frame, offset, 2)) return 0;
    if (!condition) return 1;
    target = (long)opcode_pc + (int16_t)class_u16(offset);
    if (target < 0 || (uint32_t)target >= frame->code.code_length) {
        sys_error("Branch target out of range\r\n");
        return 0;
    }
    frame->pc = (uint32_t)target;
    return 1;
}

int opcode_if(Frame *frame, unsigned char opcode, uint32_t opcode_pc) {
    uint32_t right = 0;
    uint32_t left;
    int32_t a;
    int32_t b;
    int condition;

    if (opcode >= 0x9f) {
        if (!frame_pop(frame, &right)) return 0;
        opcode -= 6;
    }
    if (!frame_pop(frame, &left)) return 0;
    a = (int32_t)left;
    b = (int32_t)right;
    switch (opcode) {
    case 0x99: condition = a == b; break;
    case 0x9a: condition = a != b; break;
    case 0x9b: condition = a < b; break;
    case 0x9c: condition = a >= b; break;
    case 0x9d: condition = a > b; break;
    default: condition = a <= b; break;
    }
    return opcode_branch(frame, opcode_pc, condition);
}

int frame_array_new(Frame *frame, uint16_t length, uint8_t element_size, uint32_t *ref) {
    ArrayTable *arrays = process_arrays(frame->processes);

    if (array_new(arrays, length, element_size, ref)) return 1;
    process_gc(frame->processes);
    if (array_new(arrays, length, element_size, ref)) return 1;
    sys_error("Not enough memory for array\r\n");
    return 0;
}

int opcode_newarray(Frame *frame) {
    unsigned char atype;
    uint32_t count;
    uint32_t ref;
    uint8_t element_size;

    if (!frame_fetch(frame, &atype, 1)) return 0;
    if (8 == atype) {
        element_size = 1;
    } else if (10 == atype) {
        element_size = 4;
    } else {
        sys_error("Unsupported array type ");
        sys_error_value(atype);
        sys_error("\r\n");
        return 0;
    }
    if (!frame_pop(frame, &count)) return 0;
    if (count > 0xffffUL) {
        sys_error("Invalid array length\r\n");
        return 0;
    }
    if (!frame_array_new(frame, (uint16_t)count, element_size, &ref)) return 0;
    return frame_push(frame, ref);
}

HeapArray *frame_array(Frame *frame, uint32_t ref, uint8_t element_size, uint32_t index) {
    HeapArray *array = array_get(process_arrays(frame->processes), ref);

    if (NULL == array) return NULL;
    if (array->element_size != element_size) {
        sys_error("Invalid array element type\r\n");
        return NULL;
    }
    if (index >= array->length) {
        sys_error("Array index out of bounds\r\n");
        return NULL;
    }
    return array;
}

int opcode_array_load(Frame *frame, uint8_t element_size) {
    uint32_t index;
    uint32_t ref;
    HeapArray *array;

    if (!frame_pop(frame, &index) || !frame_pop(frame, &ref)) return 0;
    array = frame_array(frame, ref, element_size, index);
    if (NULL == array) return 0;
    return frame_push(frame, array_read(array, (uint16_t)index));
}

int opcode_array_store(Frame *frame, uint8_t element_size) {
    uint32_t value;
    uint32_t index;
    uint32_t ref;
    HeapArray *array;

    if (!frame_pop(frame, &value) || !frame_pop(frame, &index) ||
        !frame_pop(frame, &ref)) return 0;
    array = frame_array(frame, ref, element_size, index);
    if (NULL == array) return 0;
    array_write(array, (uint16_t)index, value);
    return 1;
}

int frame_begin(Frame *frame, ProcessTable *processes, ClassPath *class_path,
    ConstantPoolCache *pools, MethodCache *methods,
    const char *class_name, const char *signature,
    const uint32_t *args, int arg_count) {
    MethodLocation *method;
    int i;

    if (!method_find(methods, class_path, pools, class_name, signature, &method)) return 0;
    if (class_path_find(class_path, class_name, &frame->location) <= 0) return 0;
    if (!constant_pool_load(pools, class_path, &frame->location, &frame->pool)) return 0;
    if (!code_open(class_path, &frame->location, method, &frame->code)) return 0;
    if (frame->code.max_stack > FRAME_STACK_SIZE ||
        frame->code.max_locals > FRAME_LOCALS_SIZE) {
        sys_error("Method frame is too large: ");
        sys_error(signature);
        sys_error("\r\n");
        return 0;
    }

    frame->processes = processes;
    frame->class_path = class_path;
    frame->pools = pools;
    frame->methods = methods;
    frame->pc = 0;
    frame->sp = 0;
    for (i = 0; i < FRAME_LOCALS_SIZE; i++) frame->locals[i] = 0;
    for (i = 0; i < arg_count && i < (int)frame->code.max_locals; i++) {
        frame->locals[i] = args[i];
    }
    return 1;
}

int opcode_static_field(Frame *frame, int store) {
    unsigned char operand[2];
    char class_name[MAX_UTF8_NAME];
    char field_name[MAX_UTF8_NAME];
    const Constant *field_ref;
    const Constant *class_entry;
    const Constant *name_and_type;
    uint32_t *value;

    if (!frame_fetch(frame, operand, 2)) return 0;
    field_ref = frame_constant(frame, class_u16(operand), 9);
    if (NULL == field_ref) return 0;
    class_entry = frame_constant(frame, class_u16(field_ref->data), 7);
    name_and_type = frame_constant(frame, class_u16(field_ref->data + 2), 12);
    if (NULL == class_entry || NULL == name_and_type) return 0;
    if (!frame_utf8(frame, class_u16(class_entry->data + 2),
        class_name, sizeof(class_name))) return 0;
    if (!frame_utf8(frame, class_u16(name_and_type->data),
        field_name, sizeof(field_name))) return 0;

    value = field_slot(process_fields(frame->processes),
        method_signature_crc(class_name), method_signature_crc(field_name));
    if (NULL == value) return 0;
    if (store) return frame_pop(frame, value);
    return frame_push(frame, *value);
}

int opcode_invokestatic(Process *process, Frame *frame) {
    unsigned char operand[2];
    char class_name[MAX_UTF8_NAME];
    char signature[MAX_UTF8_NAME];
    const Constant *method_ref;
    const Constant *class_entry;
    const Constant *name_and_type;
    const char *descriptor;
    uint32_t args[FRAME_LOCALS_SIZE];
    size_t name_length;
    int slots;
    int i;

    if (!frame_fetch(frame, operand, 2)) return 0;
    method_ref = frame_constant(frame, class_u16(operand), 10);
    if (NULL == method_ref) return 0;
    class_entry = frame_constant(frame, class_u16(method_ref->data), 7);
    name_and_type = frame_constant(frame, class_u16(method_ref->data + 2), 12);
    if (NULL == class_entry || NULL == name_and_type) return 0;
    if (!frame_utf8(frame, class_u16(class_entry->data + 2),
        class_name, sizeof(class_name))) return 0;
    if (!frame_utf8(frame, class_u16(name_and_type->data),
        signature, sizeof(signature))) return 0;
    name_length = strlen(signature);
    if (!frame_utf8(frame, class_u16(name_and_type->data + 2),
        signature + name_length, sizeof(signature) - name_length)) return 0;
    descriptor = signature + name_length;

    slots = descriptor_arg_slots(descriptor);
    if (slots < 0 || slots > FRAME_LOCALS_SIZE) {
        sys_error("Unsupported method descriptor: ");
        sys_error(descriptor);
        sys_error("\r\n");
        return 0;
    }
    for (i = slots - 1; i >= 0; i--) {
        if (!frame_pop(frame, &args[i])) return 0;
    }

    if (0 == strcmp(class_name, "platform/Native")) {
        return native_invoke(frame, signature, args);
    }

    if (process_call(frame->processes, class_name, signature, args, slots,
        process->index) < 0) return 0;
    process->blocked = 1;
    return 1;
}

int opcode_step(Process *process) {
    Frame *frame = &process->frame;
    unsigned char opcode;
    unsigned char operand[2];
    uint32_t opcode_pc;
    uint32_t value;
    HeapArray *array;

    if (!constant_pool_load(frame->pools, frame->class_path, &frame->location,
        &frame->pool)) return 0;
    if (frame->pc >= frame->code.code_length) {
        sys_error("Bytecode ended without return\r\n");
        return 0;
    }
    if (!class_seek(&frame->code.class_file,
        frame->code.code_start + (long)frame->pc)) {
        sys_error("Truncated bytecode\r\n");
        return 0;
    }
    opcode_pc = frame->pc;
    if (!frame_fetch(frame, &opcode, 1)) return 0;

    switch (opcode) {
    case 0x00:
        return 1;
    case 0x01:
        return frame_push(frame, 0);
    case 0x02: case 0x03: case 0x04: case 0x05:
    case 0x06: case 0x07: case 0x08:
        return frame_push(frame, (uint32_t)((int32_t)opcode - 0x03));
    case 0x10:
        if (!frame_fetch(frame, operand, 1)) return 0;
        return frame_push(frame, (uint32_t)(int32_t)(int8_t)operand[0]);
    case 0x11:
        if (!frame_fetch(frame, operand, 2)) return 0;
        return frame_push(frame, (uint32_t)(int32_t)(int16_t)class_u16(operand));
    case 0x12:
        return opcode_ldc(frame);
    case 0x15:
    case 0x19:
        if (!frame_fetch(frame, operand, 1)) return 0;
        return opcode_load(frame, operand[0]);
    case 0x1a: case 0x1b: case 0x1c: case 0x1d:
        return opcode_load(frame, opcode - 0x1a);
    case 0x2a: case 0x2b: case 0x2c: case 0x2d:
        return opcode_load(frame, opcode - 0x2a);
    case 0x2e:
        return opcode_array_load(frame, 4);
    case 0x33:
        return opcode_array_load(frame, 1);
    case 0x36:
    case 0x3a:
        if (!frame_fetch(frame, operand, 1)) return 0;
        return opcode_store(frame, operand[0]);
    case 0x3b: case 0x3c: case 0x3d: case 0x3e:
        return opcode_store(frame, opcode - 0x3b);
    case 0x4b: case 0x4c: case 0x4d: case 0x4e:
        return opcode_store(frame, opcode - 0x4b);
    case 0x4f:
        return opcode_array_store(frame, 4);
    case 0x54:
        return opcode_array_store(frame, 1);
    case 0x57:
        return frame_pop(frame, &value);
    case 0x59:
        if (frame->sp <= 0) {
            sys_error("Operand stack underflow\r\n");
            return 0;
        }
        return frame_push(frame, frame->stack[frame->sp - 1]);
    case 0x60: case 0x64: case 0x68: case 0x6c: case 0x70: case 0x74:
    case 0x78: case 0x7a: case 0x7c: case 0x7e: case 0x80: case 0x82:
        return opcode_math(frame, opcode);
    case 0x84:
        if (!frame_fetch(frame, operand, 2)) return 0;
        if (!frame_local_valid(frame, operand[0])) return 0;
        frame->locals[operand[0]] += (uint32_t)(int32_t)(int8_t)operand[1];
        return 1;
    case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e:
    case 0x9f: case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4:
        return opcode_if(frame, opcode, opcode_pc);
    case 0xa5:
        return opcode_if(frame, 0x9f, opcode_pc);
    case 0xa6:
        return opcode_if(frame, 0xa0, opcode_pc);
    case 0xa7:
        return opcode_branch(frame, opcode_pc, 1);
    case 0xc6:
        return opcode_if(frame, 0x99, opcode_pc);
    case 0xc7:
        return opcode_if(frame, 0x9a, opcode_pc);
    case 0xac:
    case 0xb0:
        if (!frame_pop(frame, &value)) return 0;
        return process_return(frame->processes, process, 1, value);
    case 0xb1:
        return process_return(frame->processes, process, 0, 0);
    case 0xb2:
        return opcode_static_field(frame, 0);
    case 0xb3:
        return opcode_static_field(frame, 1);
    case 0xb8:
        return opcode_invokestatic(process, frame);
    case 0xbc:
        return opcode_newarray(frame);
    case 0xbe:
        if (!frame_pop(frame, &value)) return 0;
        array = array_get(process_arrays(frame->processes), value);
        if (NULL == array) return 0;
        return frame_push(frame, array->length);
    default:
        sys_error("Unsupported opcode ");
        sys_error_value(opcode);
        sys_error(" at pc ");
        sys_error_value(opcode_pc);
        sys_error("\r\n");
        return 0;
    }
}
