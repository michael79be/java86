#include <stdint.h>
#include <string.h>

#include "sys.c"
#include "jar.c"
#include "class.c"
#include "cpool.c"
#include "method.c"
#include "array.c"
#include "field.c"
#include "opcode.c"
#include "math.c"
#include "native.c"
#include "proc.c"

typedef struct {
    ClassPath class_path;
    ConstantPoolCache constant_pools;
    MethodCache methods;
    ProcessTable processes;
} VM;

void vm_init(VM *vm) {
    class_path_init(&vm->class_path);
    constant_pool_cache_init(&vm->constant_pools);
    method_cache_init(&vm->methods);
    process_table_init(&vm->processes, &vm->class_path,
        &vm->constant_pools, &vm->methods);
}

void vm_destroy(VM *vm) {
    process_table_destroy(&vm->processes);
    constant_pool_cache_destroy(&vm->constant_pools);
    class_path_close(&vm->class_path);
}

int run_method(VM *vm, const char *class_name, const char *signature) {
    if (process_call(&vm->processes, class_name, signature, NULL, 0, -1) < 0) return 0;
    return process_run(&vm->processes);
}

int main(int argc, char *argv[]) {
    static VM vm;

    if (4 != argc || 0 != strcmp(argv[1], "-classpath")) {
        sys_error("Usage: java.com -classpath <file1.jar;file2.jar> <classname>\r\n");
        return 1;
    }

    vm_init(&vm);
    if (!class_path_open(&vm.class_path, argv[2])) {
        vm_destroy(&vm);
        return 1;
    }

    sys_print("Java86 VM starting...\r\n");
    sys_print("Loading class: ");
    sys_print(argv[3]);
    sys_print("\r\n");

    if (!run_method(&vm, argv[3], "main([Ljava/lang/String;)V")) {
        vm_destroy(&vm);
        return 1;
    }

    vm_destroy(&vm);
    return 0;
}
