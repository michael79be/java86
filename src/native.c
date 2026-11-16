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

static void set_VESA_bank(uint32_t bank) {
    interrupt_registers[0] = 0x4F05;
    interrupt_registers[1] = 0x0000;
    interrupt_registers[3] = (uint16_t)bank;
    interrupt_thunk[1] = 0x10;
    interrupt_invoke();
}
#else
static uint8_t host_memory[0x100000];

ArrayData memory_pointer(uint32_t address) {
    return &host_memory[address & 0xfffffUL];
}

static void set_VESA_bank(uint32_t bank) { (void)bank; }
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

int native_read_bytes(Frame *frame, uint32_t address, uint32_t reference, uint32_t windowWidth, uint32_t frameWidth, uint32_t offset) {
    HeapArray *array = array_get(process_arrays(frame->processes), reference);
    if (NULL == array || 1 != array->element_size) {
        sys_error("Invalid byte array reference\r\n");
        return 0;
    }
    if (frameWidth == 0) {
        sys_error("frameWidth must be > 0\r\n");
        return 0;
    }
    uint32_t rows = array->length / frameWidth;
    int32_t current_bank = -1;

    for (uint32_t r = 0; r < rows; r++) {
        uint32_t srcBase = address + r * windowWidth + offset;

        if (srcBase >= 0xa0000) {
            uint32_t linear_start = srcBase - 0xa0000;
            uint32_t linear_end = linear_start + frameWidth - 1;
            uint32_t bank_start = linear_start >> 16;
            uint32_t bank_end = linear_end >> 16;

            if (bank_start == bank_end) {
                if (current_bank != (int32_t)bank_start) {
                    set_VESA_bank(bank_start);
                    current_bank = bank_start;
                }
                for (uint16_t c = 0; c < frameWidth; c++) {
                    uint8_t val = memory_pointer(0xa0000 + (linear_start & 0xffff) + c)[0];
                    array_write(array, r * frameWidth + c, (uint32_t)val);
                }
            } else {
                uint32_t split_c = 65536 - (linear_start & 0xffff);

                if (current_bank != (int32_t)bank_start) {
                    set_VESA_bank(bank_start);
                    current_bank = bank_start;
                }
                for (uint16_t c = 0; c < split_c; c++) {
                    uint8_t val = memory_pointer(0xa0000 + (linear_start & 0xffff) + c)[0];
                    array_write(array, r * frameWidth + c, (uint32_t)val);
                }

                if (current_bank != (int32_t)bank_end) {
                    set_VESA_bank(bank_end);
                    current_bank = bank_end;
                }
                for (uint16_t c = split_c; c < frameWidth; c++) {
                    uint8_t val = memory_pointer(0xa0000 + (c - split_c))[0];
                    array_write(array, r * frameWidth + c, (uint32_t)val);
                }
            }
        } else {
            for (uint16_t c = 0; c < frameWidth; c++) {
                uint8_t val = memory_pointer(srcBase + c)[0];
                array_write(array, r * frameWidth + c, (uint32_t)val);
            }
        }
    }
    return 1;
}

int native_write_bytes(Frame *frame, uint32_t address, uint32_t reference, uint32_t windowWidth, uint32_t frameWidth, uint32_t offset, int32_t ignoreColor) {
    HeapArray *array = array_get(process_arrays(frame->processes), reference);
    if (NULL == array || 1 != array->element_size) {
        sys_error("Invalid byte array reference\r\n");
        return 0;
    }
    if (frameWidth == 0) {
        sys_error("frameWidth must be > 0\r\n");
        return 0;
    }

    uint32_t rows = array->length / frameWidth;
    int32_t current_bank = -1;

    for (uint32_t r = 0; r < rows; r++) {
        uint32_t destBase = address + r * windowWidth + offset;

        if (destBase >= 0xa0000) {
            uint32_t linear_start = destBase - 0xa0000;
            uint32_t linear_end = linear_start + frameWidth - 1;
            uint32_t bank_start = linear_start >> 16;
            uint32_t bank_end = linear_end >> 16;

            if (bank_start == bank_end) {
                if (current_bank != (int32_t)bank_start) {
                    set_VESA_bank(bank_start);
                    current_bank = bank_start;
                }
                for (uint16_t c = 0; c < frameWidth; c++) {
                    uint8_t val = (uint8_t)array_read(array, r * frameWidth + c);
                    if (ignoreColor != -1 && val == (uint8_t)ignoreColor) continue;
                    memory_pointer(0xa0000 + (linear_start & 0xffff) + c)[0] = val;
                }
            } else {
                uint32_t split_c = 65536 - (linear_start & 0xffff);

                if (current_bank != (int32_t)bank_start) {
                    set_VESA_bank(bank_start);
                    current_bank = bank_start;
                }
                for (uint16_t c = 0; c < split_c; c++) {
                    uint8_t val = (uint8_t)array_read(array, r * frameWidth + c);
                    if (ignoreColor != -1 && val == (uint8_t)ignoreColor) continue;
                    memory_pointer(0xa0000 + (linear_start & 0xffff) + c)[0] = val;
                }

                if (current_bank != (int32_t)bank_end) {
                    set_VESA_bank(bank_end);
                    current_bank = bank_end;
                }
                for (uint16_t c = split_c; c < frameWidth; c++) {
                    uint8_t val = (uint8_t)array_read(array, r * frameWidth + c);
                    if (ignoreColor != -1 && val == (uint8_t)ignoreColor) continue;
                    memory_pointer(0xa0000 + (c - split_c))[0] = val;
                }
            }
        } else {
            for (uint16_t c = 0; c < frameWidth; c++) {
                uint8_t val = (uint8_t)array_read(array, r * frameWidth + c);
                if (ignoreColor != -1 && val == (uint8_t)ignoreColor) continue;
                memory_pointer(destBase + c)[0] = val;
            }
        }
    }
    return 1;
}

int native_fill_bytes(Frame *frame, uint32_t address, uint32_t value, uint32_t count) {
    if (address >= 0xa0000) {
        uint32_t linear_start = address - 0xa0000;
        uint32_t current_pos = 0;
        int32_t current_bank = -1;

        while (current_pos < count) {
            uint32_t abs_linear = linear_start + current_pos;
            uint32_t bank = abs_linear >> 16;
            uint32_t offset = abs_linear & 0xffff;
            uint32_t chunk = 65536 - offset;
            if (current_pos + chunk > count) {
                chunk = count - current_pos;
            }

            if (current_bank != (int32_t)bank) {
                set_VESA_bank(bank);
                current_bank = (int32_t)bank;
            }

            for (uint32_t i = 0; i < chunk; i++) {
                memory_pointer(0xa0000 + offset + i)[0] = (uint8_t)value;
            }
            current_pos += chunk;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            memory_pointer(address + i)[0] = (uint8_t)value;
        }
    }
    return 1;
}

#ifdef __ia16__
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a" (ret) : "Nd" (port));
    return ret;
}
#endif

int native_read_port(Frame *frame, uint32_t port) {
#ifdef __ia16__
    return frame_push(frame, inb((uint16_t)port));
#else
    (void)port;
    return frame_push(frame, 0);
#endif
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

static void write_byte_mapped(uint32_t address, uint8_t value) {
    if (address >= 0xa0000) {
        uint32_t linear = address - 0xa0000;
        set_VESA_bank(linear >> 16);
        memory_pointer(0xa0000 + (linear & 0xffff))[0] = value;
    } else {
        memory_pointer(address)[0] = value;
    }
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
        write_byte_mapped(args[0], (uint8_t)args[1]);
        return 1;
    }
    if (0 == strcmp(signature, "readBytes(I[BIII)V")) {
        return native_read_bytes(frame, args[0], args[1], args[2], args[3], args[4]);
    }
    if (0 == strcmp(signature, "writeBytes(I[BIIII)V")) {
        return native_write_bytes(frame, args[0], args[1], args[2], args[3], args[4], args[5]);
    }
    if (0 == strcmp(signature, "writeBytes(III)V")) {
        return native_fill_bytes(frame, (uint32_t)args[0], (uint32_t)args[1], (uint32_t)args[2]);
    }
    if (0 == strcmp(signature, "readPort(I)I")) {
        return native_read_port(frame, args[0]);
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
