// main.c
#include <stdio.h>
#include <string.h>
#include "vm.h"
#include "parser.h"
#include "jit.h"

int main() {
    char expr[MAX_EXPR_LEN];
    Instruction program[MAX_OP_NR];
    
    printf("ByteForge JIT\n");
    printf("Enter standard math (e.g., '10 + 2 * (3 + 5)')\n");
    printf("Type 'exit' to quit.\n\n");

    while (1) {
        printf("JIT> ");
        if (!fgets(expr, sizeof(expr), stdin)) break;
        
        expr[strcspn(expr, "\n")] = 0;
        if (strcmp(expr, "exit") == 0) break;

        // Skip compilation if the parser failed or the string was empty
        if (parse_expression(expr, program) == 0 && program[0].op != OP_END) {
            int64_t result = execute_jit(program);
            printf("= %lld\n", (long long)result);
        }
    }
    return 0;
}
