#include <stdlib.h>
#include <stdint.h>

#define MAX_ARRAYS 16
#define ARRAY_REF_FLAG 0x80000000UL
#define ARRAY_MAX_BYTES 0xfff0UL

#ifdef __ia16__
typedef uint8_t __far *ArrayData;
#else
typedef uint8_t *ArrayData;
#endif

typedef struct {
    uint8_t *data;
    uint16_t segment;
    uint16_t length;
    uint8_t element_size;
    uint8_t used;
    uint8_t marked;
} HeapArray;

typedef struct {
    HeapArray arrays[MAX_ARRAYS];
} ArrayTable;

#ifdef __ia16__
uint16_t dos_alloc(uint16_t paragraphs) {
    uint16_t segment;
    uint16_t failed;

    __asm__ volatile (
        "movb $0x48, %%ah\n\t"
        "int $0x21\n\t"
        "sbbw %%bx, %%bx"
        : "=a" (segment), "=b" (failed)
        : "1" (paragraphs)
        : "cc", "memory");
    return failed ? 0 : segment;
}

void dos_free(uint16_t segment) {
    __asm__ volatile (
        "pushw %%es\n\t"
        "movw %w0, %%es\n\t"
        "movb $0x49, %%ah\n\t"
        "int $0x21\n\t"
        "popw %%es"
        :
        : "r" (segment)
        : "ax", "cc", "memory");
}
#endif

ArrayData array_data(HeapArray *array) {
#ifdef __ia16__
    if (NULL == array->data) {
        return (ArrayData)((uint32_t)array->segment << 16);
    }
#endif
    return array->data;
}

void array_table_init(ArrayTable *table) {
    int i;

    for (i = 0; i < MAX_ARRAYS; i++) {
        table->arrays[i].used = 0;
    }
}

void array_release(HeapArray *array) {
    free(array->data);
#ifdef __ia16__
    if (NULL == array->data && 0 != array->segment) {
        dos_free(array->segment);
    }
#endif
    array->used = 0;
}

void array_table_destroy(ArrayTable *table) {
    int i;

    for (i = 0; i < MAX_ARRAYS; i++) {
        if (table->arrays[i].used) {
            array_release(&table->arrays[i]);
        }
    }
}

int array_new(ArrayTable *table, uint16_t length, uint8_t element_size, uint32_t *ref) {
    HeapArray *array = NULL;
    uint32_t size = (uint32_t)length * element_size;
    ArrayData data;
    uint16_t i;

    if (size > ARRAY_MAX_BYTES) return 0;
    for (i = 0; i < MAX_ARRAYS; i++) {
        if (!table->arrays[i].used) {
            array = &table->arrays[i];
            break;
        }
    }
    if (NULL == array) return 0;

    array->segment = 0;
    array->data = malloc(size ? (size_t)size : 1);
#ifdef __ia16__
    if (NULL == array->data) {
        array->segment = dos_alloc((uint16_t)((size + 15) >> 4));
        if (0 == array->segment) return 0;
    }
#else
    if (NULL == array->data) return 0;
#endif

    array->length = length;
    array->element_size = element_size;
    array->used = 1;
    data = array_data(array);
    for (i = 0; i < (uint16_t)size; i++) {
        data[i] = 0;
    }
    *ref = ARRAY_REF_FLAG | (uint32_t)(array - table->arrays);
    return 1;
}

HeapArray *array_get(ArrayTable *table, uint32_t ref) {
    uint32_t index = ref & ~ARRAY_REF_FLAG;

    if (!(ref & ARRAY_REF_FLAG) || index >= MAX_ARRAYS ||
        !table->arrays[index].used) {
        sys_error("Invalid array reference\r\n");
        return NULL;
    }
    return &table->arrays[index];
}

uint32_t array_read(HeapArray *array, uint16_t index) {
    ArrayData data = array_data(array);

    data += (uint16_t)((uint32_t)index * array->element_size);
    if (1 == array->element_size) {
        return (uint32_t)(int32_t)(int8_t)data[0];
    }
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

void array_write(HeapArray *array, uint16_t index, uint32_t value) {
    ArrayData data = array_data(array);

    data += (uint16_t)((uint32_t)index * array->element_size);
    data[0] = (uint8_t)value;
    if (4 == array->element_size) {
        data[1] = (uint8_t)(value >> 8);
        data[2] = (uint8_t)(value >> 16);
        data[3] = (uint8_t)(value >> 24);
    }
}

void array_mark_reset(ArrayTable *table) {
    int i;

    for (i = 0; i < MAX_ARRAYS; i++) {
        table->arrays[i].marked = 0;
    }
}

void array_mark(ArrayTable *table, uint32_t ref) {
    uint32_t index = ref & ~ARRAY_REF_FLAG;

    if ((ref & ARRAY_REF_FLAG) && index < MAX_ARRAYS &&
        table->arrays[index].used) {
        table->arrays[index].marked = 1;
    }
}

int array_sweep(ArrayTable *table) {
    int freed = 0;
    int i;

    for (i = 0; i < MAX_ARRAYS; i++) {
        if (table->arrays[i].used && !table->arrays[i].marked) {
            array_release(&table->arrays[i]);
            freed++;
        }
    }
    return freed;
}
