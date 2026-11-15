#include <stdint.h>
#include <string.h>

#ifdef __ia16__
static volatile uint16_t interrupt_registers[9];
static unsigned char interrupt_thunk[3] = { 0xcd, 0x00, 0xc3 };

void interrupt_invoke(void) {
    __asm__ volatile (
        "pushw %%ds\n\t"
        "pushw %%es\n\t"
        "pushw %%si\n\t"
        "pushw %%di\n\t"
        "movw %%cs:interrupt_registers+2, %%bx\n\t"
        "movw %%cs:interrupt_registers+4, %%cx\n\t"
        "movw %%cs:interrupt_registers+6, %%dx\n\t"
        "movw %%cs:interrupt_registers+8, %%si\n\t"
        "movw %%cs:interrupt_registers+10, %%di\n\t"
        "movw %%cs:interrupt_registers+14, %%es\n\t"
        "movw %%cs:interrupt_registers+12, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%cs:interrupt_registers+0, %%ax\n\t"
        "call interrupt_thunk\n\t"
        "movw %%ax, %%cs:interrupt_registers+0\n\t"
        "movw %%bx, %%cs:interrupt_registers+2\n\t"
        "movw %%cx, %%cs:interrupt_registers+4\n\t"
        "movw %%dx, %%cs:interrupt_registers+6\n\t"
        "movw %%si, %%cs:interrupt_registers+8\n\t"
        "movw %%di, %%cs:interrupt_registers+10\n\t"
        "movw %%ds, %%ax\n\t"
        "movw %%ax, %%cs:interrupt_registers+12\n\t"
        "movw %%es, %%ax\n\t"
        "movw %%ax, %%cs:interrupt_registers+14\n\t"
        "pushfw\n\t"
        "popw %%ax\n\t"
        "movw %%ax, %%cs:interrupt_registers+16\n\t"
        "popw %%di\n\t"
        "popw %%si\n\t"
        "popw %%es\n\t"
        "popw %%ds"
        ::: "ax", "bx", "cx", "dx", "cc", "memory");
}

ArrayData memory_pointer(uint32_t address) {
    return (ArrayData)(((address >> 4) << 16) | (address & 0xf));
}
#else
static uint8_t host_memory[0x100000];

ArrayData memory_pointer(uint32_t address) {
    return &host_memory[address & 0xfffffUL];
}
#endif

int native_new_process(Frame *frame, uint32_t reference) {
    char class_name[MAX_UTF8_NAME];
    int id;

    if (!string_read(frame->class_path, reference, class_name, sizeof(class_name))) {
        sys_error("Invalid string reference\r\n");
        return 0;
    }
    id = process_spawn(frame->processes, class_name);
    if (id < 0) return 0;
    return frame_push(frame, (uint32_t)id);
}

int native_to_string(Frame *frame, uint32_t reference) {
    HeapArray *array = array_get(process_arrays(frame->processes), reference);

    if (NULL == array || 1 != array->element_size) {
        sys_error("Invalid byte array reference\r\n");
        return 0;
    }
    return frame_push(frame, reference);
}

int native_get_bytes(Frame *frame, uint32_t reference) {
    int file;
    uint16_t length;
    uint16_t i;
    uint32_t ref;
    HeapArray *array;
    unsigned char character;

    if (reference & ARRAY_REF_FLAG) {
        return native_to_string(frame, reference);
    }

    if (!string_open(frame->class_path, reference, &file, &length)) {
        sys_error("Invalid string reference\r\n");
        return 0;
    }
    if (!frame_array_new(frame, length, 1, &ref)) return 0;
    array = array_get(process_arrays(frame->processes), ref);
    if (NULL == array) return 0;
    for (i = 0; i < length; i++) {
        if (!sys_read_byte(file, &character)) {
            sys_error("Invalid string reference\r\n");
            return 0;
        }
        array_write(array, i, (uint32_t)character);
    }
    return frame_push(frame, ref);
}

int native_read_bytes(Frame *frame, uint32_t address, uint32_t reference) {
    HeapArray *array = array_get(process_arrays(frame->processes), reference);
    uint16_t i;

    if (NULL == array || 1 != array->element_size) {
        sys_error("Invalid byte array reference\r\n");
        return 0;
    }
    for (i = 0; i < array->length; i++) {
        array_write(array, i, memory_pointer(address + i)[0]);
    }
    return 1;
}

int native_write_bytes(Frame *frame, uint32_t address, uint32_t reference) {
    HeapArray *array = array_get(process_arrays(frame->processes), reference);
    uint16_t i;

    if (NULL == array || 1 != array->element_size) {
        sys_error("Invalid byte array reference\r\n");
        return 0;
    }
    for (i = 0; i < array->length; i++) {
        memory_pointer(address + i)[0] = (uint8_t)array_read(array, i);
    }
    return 1;
}

int native_get_address(Frame *frame, uint32_t reference) {
    HeapArray *array = array_get(process_arrays(frame->processes), reference);

    if (NULL == array || 1 != array->element_size) {
        sys_error("Invalid byte array reference\r\n");
        return 0;
    }
#ifdef __ia16__
    if (NULL == array->data) {
        return frame_push(frame, (uint32_t)array->segment << 4);
    }
    {
        uint16_t data_segment;
        __asm__ ("movw %%ds, %0" : "=r" (data_segment));
        return frame_push(frame, ((uint32_t)data_segment << 4) +
            (uint16_t)(uintptr_t)array->data);
    }
#else
    sys_error("Memory addresses require FreeDOS\r\n");
    return 0;
#endif
}

int native_interrupt(Frame *frame, uint32_t number, uint32_t reference) {
    HeapArray *regs = array_get(process_arrays(frame->processes), reference);

    if (NULL == regs || 4 != regs->element_size || regs->length < 9) {
        sys_error("Invalid interrupt registers array\r\n");
        return 0;
    }
#ifdef __ia16__
    {
        uint16_t i;
        for (i = 0; i < 9; i++) {
            interrupt_registers[i] = (uint16_t)array_read(regs, i);
        }
        interrupt_thunk[1] = (uint8_t)number;
        interrupt_invoke();
        for (i = 0; i < 9; i++) {
            array_write(regs, i, interrupt_registers[i]);
        }
    }
    return frame_push(frame, reference);
#else
    (void)number;
    sys_error("DOS interrupts require FreeDOS\r\n");
    return 0;
#endif
}

int native_invoke(Frame *frame, const char *signature, const uint32_t *args) {
    if (0 == strcmp(signature, "newProcess(Ljava/lang/String;)I")) {
        return native_new_process(frame, args[0]);
    }
    if (0 == strcmp(signature, "killProcess(I)V")) {
        return process_kill(frame->processes, (int)(int32_t)args[0]);
    }
    if (0 == strcmp(signature, "toString([B)Ljava/lang/String;")) {
        return native_to_string(frame, args[0]);
    }
    if (0 == strcmp(signature, "getBytes(Ljava/lang/String;)[B")) {
        return native_get_bytes(frame, args[0]);
    }
    if (0 == strcmp(signature, "readByte(I)I")) {
        return frame_push(frame, memory_pointer(args[0])[0]);
    }
    if (0 == strcmp(signature, "writeByte(II)V")) {
        memory_pointer(args[0])[0] = (uint8_t)args[1];
        return 1;
    }
    if (0 == strcmp(signature, "readBytes(I[B)V")) {
        return native_read_bytes(frame, args[0], args[1]);
    }
    if (0 == strcmp(signature, "writeBytes(I[B)V")) {
        return native_write_bytes(frame, args[0], args[1]);
    }
    if (0 == strcmp(signature, "getAddress([B)I")) {
        return native_get_address(frame, args[0]);
    }
    if (0 == strcmp(signature, "interrupt(I[I)[I")) {
        return native_interrupt(frame, args[0], args[1]);
    }
    sys_error("Unsupported native method: ");
    sys_error(signature);
    sys_error("\r\n");
    return 0;
}
