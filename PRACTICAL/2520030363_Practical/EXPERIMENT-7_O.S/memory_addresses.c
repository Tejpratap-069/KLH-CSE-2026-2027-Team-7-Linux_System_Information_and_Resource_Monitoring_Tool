#include <stdio.h>
#include <stdlib.h>

int global_var = 100;

void code_function() {
    printf("Code address   : %p\n", (void *)code_function);
}

int main() {
    static int static_var = 200;
    int stack_var = 300;
    int *heap_var = malloc(sizeof(int));

    if (heap_var == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    *heap_var = 400;

    printf("Global address : %p\n", (void *)&global_var);
    printf("Static address : %p\n", (void *)&static_var);
    printf("Stack address  : %p\n", (void *)&stack_var);
    printf("Heap address   : %p\n", (void *)heap_var);

    code_function();

    free(heap_var);

    return 0;
}
