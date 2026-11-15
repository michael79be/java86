#define MAX_PROCESSES 10

struct ProcessTable {
    Process processes[MAX_PROCESSES];
    ArrayTable arrays;
    FieldTable fields;
    ClassPath *class_path;
    ConstantPoolCache *pools;
    MethodCache *methods;
};

ArrayTable *process_arrays(ProcessTable *table) {
    return &table->arrays;
}

FieldTable *process_fields(ProcessTable *table) {
    return &table->fields;
}

void process_table_init(ProcessTable *table, ClassPath *class_path,
    ConstantPoolCache *pools, MethodCache *methods) {
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        table->processes[i].index = i;
        table->processes[i].active = 0;
    }
    array_table_init(&table->arrays);
    field_table_init(&table->fields);
    table->class_path = class_path;
    table->pools = pools;
    table->methods = methods;
}

void process_table_destroy(ProcessTable *table) {
    array_table_destroy(&table->arrays);
}

int process_gc(ProcessTable *table) {
    int i;
    int j;

    array_mark_reset(&table->arrays);
    for (i = 0; i < MAX_FIELDS; i++) {
        if (table->fields.fields[i].used) {
            array_mark(&table->arrays, table->fields.fields[i].value);
        }
    }
    for (i = 0; i < MAX_PROCESSES; i++) {
        Frame *frame = &table->processes[i].frame;
        if (!table->processes[i].active) continue;
        for (j = 0; j < frame->sp; j++) {
            array_mark(&table->arrays, frame->stack[j]);
        }
        for (j = 0; j < FRAME_LOCALS_SIZE; j++) {
            array_mark(&table->arrays, frame->locals[j]);
        }
    }
    return array_sweep(&table->arrays);
}

int process_call(ProcessTable *table, const char *class_name, const char *signature,
    const uint32_t *args, int arg_count, int parent) {
    Process *process = NULL;
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!table->processes[i].active) {
            process = &table->processes[i];
            break;
        }
    }
    if (NULL == process) {
        sys_error("Too many processes: ");
        sys_error(class_name);
        sys_error("\r\n");
        return -1;
    }
    if (!frame_begin(&process->frame, table, table->class_path,
        table->pools, table->methods, class_name, signature, args, arg_count)) {
        return -1;
    }
    process->parent = parent;
    process->active = 1;
    process->blocked = 0;
    return process->index;
}

int process_spawn(ProcessTable *table, const char *class_name) {
    return process_call(table, class_name, "main([Ljava/lang/String;)V", NULL, 0, -1);
}

int process_return(ProcessTable *table, Process *process, int has_value, uint32_t value) {
    process->active = 0;
    if (process->parent >= 0) {
        Process *parent = &table->processes[process->parent];
        parent->blocked = 0;
        if (has_value) return frame_push(&parent->frame, value);
    }
    return 1;
}

void process_kill_children(ProcessTable *table, int parent) {
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (table->processes[i].active && table->processes[i].parent == parent) {
            table->processes[i].active = 0;
            process_kill_children(table, i);
        }
    }
}

int process_kill(ProcessTable *table, int id) {
    Process *process;

    if (id < 0 || id >= MAX_PROCESSES) {
        sys_error("Invalid process id\r\n");
        return 0;
    }
    process = &table->processes[id];
    if (!process->active) return 1;
    process->active = 0;
    if (process->parent >= 0) {
        table->processes[process->parent].blocked = 0;
    }
    process_kill_children(table, id);
    return 1;
}

int process_run(ProcessTable *table) {
    int alive = 1;
    int i;

    while (alive) {
        alive = 0;
        for (i = 0; i < MAX_PROCESSES; i++) {
            Process *process = &table->processes[i];
            if (!process->active) continue;
            alive = 1;
            if (process->blocked) continue;
            if (!opcode_step(process)) return 0;
        }
    }
    return 1;
}
