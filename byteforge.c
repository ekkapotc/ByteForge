#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/mman.h>

typedef enum {
    OP_PUSH,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_REM,
    OP_END
} Opcode;

typedef struct {
    Opcode op;
    int32_t arg; //only for OP_PUSH
} Instruction;

void emit(uint8_t **jit_ptr, const uint8_t *code, size_t len) {
    memcpy(*jit_ptr, code, len);
    *jit_ptr += len; 
}

void emit_push(uint8_t **jit_ptr, int32_t imm4) {
    // mov rax, <imm4>
    uint8_t mov_rax[] = {0x48, 0xC7, 0xC0};
    emit(jit_ptr, mov_rax, 3); //move rax takes 3 bytes long
    emit(jit_ptr, (uint8_t*)&imm4, 4); //<imm4> is 4 bytes long
    // push rax
    uint8_t push_rax = 0x50; 
    emit(jit_ptr, &push_rax, 1); 
}

void emit_add(uint8_t **jit_ptr) {
    // pop rcx;
    // pop rax; 
    // add rax, rcx; 
    // push rax
    uint8_t code[] = {0x59, 0x58, 0x48, 0x01, 0xC8, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_sub(uint8_t **jit_ptr) {
    // pop rcx; 
    // pop rax; 
    // sub rax, rcx; 
    // push rax
    uint8_t code[] = {0x59, 0x58, 0x48, 0x29, 0xC8, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_mul(uint8_t **jit_ptr) {
    // pop rcx; 
    // pop rax; 
    // imul rax, rcx; 
    // push rax
    uint8_t code[] = {0x59, 0x58, 0x48, 0x0F, 0xAF, 0xC1, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_div(uint8_t **jit_ptr) {
    // pop rcx;
    // pop rax;
    // cqo;
    // idiv rcx;
    // push rax;
    uint8_t code[] = {0x59, 0x58, 0x48, 0x99, 0x48, 0xF7, 0xF9, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_rem(uint8_t **jit_ptr) {
    // pop rcx;
    // pop rax;
    // cqo;
    // idiv rcx;
    // push rdx;
    uint8_t code[] = {0x59, 0x58, 0x48, 0x99, 0x48, 0xF7, 0xF9, 0x52};
    emit(jit_ptr, code, sizeof(code));
}

void emit_ret(uint8_t **jit_ptr) {
    // pop rax; 
    // ret  
    // Note: rax stores the return value by the AMD64 calling convention
    uint8_t code[] = {0x58, 0xC3};
    emit(jit_ptr, code, sizeof(code));
}

void* allocate_jit_memory(size_t size) {
    // Allocate memory as Read & Write first
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    return ptr;
}

void make_memory_executable(void* ptr, size_t size) {
    // Toggle memory to Read & Execute to comply with W^X security policies
    if (mprotect(ptr, size, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect failed");
        exit(1);
    }
}

int precedence(char op) {
    if (op == '%') return 4;
    if (op == '/') return 3;
    if (op == '*') return 2;
    if (op == '+' || op == '-') return 1;
    return 0;
}

#define  MAX_OP_NR 128
#define  MAX_EXPR_LEN (2*MAX_OP_NR)

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

        // --- Shunting Yard Parser ---
        int pc = 0;
        char op_stack[MAX_OP_NR];
        int op_top = -1; // -1 means empty operation stack 
        char *curr = expr;

        while (*curr && pc < MAX_OP_NR-1) {
            if (isspace(*curr)) {
                curr++; // Skip white spaces
            } 
            else if (isdigit(*curr)) {
                // Parse multi-digit numbers
                program[pc].op = OP_PUSH;
                program[pc].arg = strtol(curr, &curr, 10);
                pc++;
            } 
            else if (*curr == '(') {
                op_stack[++op_top] = *curr;
                curr++;
            } 
            else if (*curr == ')') {
                // Pop until matching '('
                while (op_top >= 0 && op_stack[op_top] != '(') {
                    char op = op_stack[op_top--];
                    if (op == '+') program[pc++].op = OP_ADD;
                    else if (op == '-') program[pc++].op = OP_SUB;
                    else if (op == '*') program[pc++].op = OP_MUL;
                    else if (op == '/') program[pc++].op = OP_DIV;
                    else if (op == '%') program[pc++].op = OP_REM;
                }
                if (op_top >= 0) op_top--; // Discard the '('
                curr++;
            } 
            else if (*curr == '+' || *curr == '-' || *curr == '*' || *curr == '/' || *curr == '%') {
                // Handle operator precedence
                while (op_top >= 0 && op_stack[op_top] != '(' && precedence(op_stack[op_top]) >= precedence(*curr)) {
                    char op = op_stack[op_top--];
                    if (op == '+') program[pc++].op = OP_ADD;
                    else if (op == '-') program[pc++].op = OP_SUB;
                    else if (op == '*') program[pc++].op = OP_MUL;
                    else if (op == '/') program[pc++].op = OP_DIV;
                    else if (op == '%') program[pc++].op = OP_REM;
		}

                op_stack[++op_top] = *curr;
                curr++;
            } 
            else {
                printf("Unknown character: %c\n", *curr);
                goto next_loop;
            }
        }

        // Pop any remaining operators from the stack
        while (op_top >= 0) {
            char op = op_stack[op_top--];
            if (op == '(') continue; // Mismatched parenthesis
            if (op == '+') program[pc++].op = OP_ADD;
            else if (op == '-') program[pc++].op = OP_SUB;
            else if (op == '*') program[pc++].op = OP_MUL;
            else if (op == '/') program[pc++].op = OP_DIV;
            else if (op == '%') program[pc++].op = OP_REM;
        }

        program[pc].op = OP_END; // Terminate VM stream

        if (pc == 0) continue;

        // --- BACKEND: Identical JIT Phase ---
        size_t mem_size = 4096;  //allocate one 4K page
        uint8_t * jit_memory = (uint8_t*) allocate_jit_memory(mem_size); 
        uint8_t * write_ptr = jit_memory;

        for (int i = 0; i <= pc; i++) {
            switch(program[i].op) {
                case OP_PUSH: emit_push(&write_ptr, program[i].arg); break;
                case OP_ADD:  emit_add(&write_ptr); break;
                case OP_SUB:  emit_sub(&write_ptr); break;
                case OP_MUL:  emit_mul(&write_ptr); break;
                case OP_DIV:  emit_div(&write_ptr); break;
		case OP_REM:  emit_rem(&write_ptr); break;
                case OP_END:  emit_ret(&write_ptr); break;
            }
        }

        make_memory_executable(jit_memory, mem_size);
        int64_t (*jit_function)() = (int64_t (*)())jit_memory;
        
        int64_t result = jit_function();
        printf("= %lld\n", (long long)result);

        munmap(jit_memory, mem_size);

next_loop:;
    }
    return 0;
}
