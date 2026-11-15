#include <stdint.h>

#define MAX_FIELDS 100

typedef struct {
    uint16_t class_crc;
    uint16_t name_crc;
    uint32_t value;
    uint8_t used;
} StaticField;

typedef struct {
    StaticField fields[MAX_FIELDS];
} FieldTable;

void field_table_init(FieldTable *table) {
    int i;

    for (i = 0; i < MAX_FIELDS; i++) {
        table->fields[i].used = 0;
    }
}

uint32_t *field_slot(FieldTable *table, uint16_t class_crc, uint16_t name_crc) {
    StaticField *field;
    int i;

    for (i = 0; i < MAX_FIELDS; i++) {
        field = &table->fields[i];
        if (field->used && field->class_crc == class_crc &&
            field->name_crc == name_crc) {
            return &field->value;
        }
    }
    for (i = 0; i < MAX_FIELDS; i++) {
        field = &table->fields[i];
        if (!field->used) {
            field->class_crc = class_crc;
            field->name_crc = name_crc;
            field->value = 0;
            field->used = 1;
            return &field->value;
        }
    }
    sys_error("Too many static fields\r\n");
    return NULL;
}
