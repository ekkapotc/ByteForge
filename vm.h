// vm.h
#ifndef VM_H
#define VM_H

#include <stdint.h>

#define MAX_OP_NR 128
#define MAX_EXPR_LEN (2*MAX_OP_NR)

typedef enum {
    OP_PUSH,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_REM,
    OP_NEG, 
    OP_END
} Opcode;

typedef struct {
    Opcode op;
    int32_t arg; 
} Instruction;

#endif // VM_H
