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
    OP_NEG, // Unary negation operator
    OP_END
} Opcode;

typedef struct {
    Opcode op;
    int32_t arg; // Argument field, utilized exclusively by the OP_PUSH instruction
} Instruction;

void emit(uint8_t **jit_ptr, const uint8_t *code, size_t len) {
    memcpy(*jit_ptr, code, len);
    *jit_ptr += len; 
}

void emit_push(uint8_t **jit_ptr, int32_t imm4) {
    // mov rax, <imm4> (Load the 32-bit immediate value into the 64-bit rax register)
    uint8_t mov_rax[] = {0x48, 0xC7, 0xC0};
    emit(jit_ptr, mov_rax, 3); // The 'mov rax' x86-64 opcode prefix requires 3 bytes
    emit(jit_ptr, (uint8_t*)&imm4, 4); // The 32-bit immediate value requires 4 bytes
    // push rax (Push the loaded value onto the hardware execution stack)
    uint8_t push_rax = 0x50; 
    emit(jit_ptr, &push_rax, 1); 
}

void emit_add(uint8_t **jit_ptr) {
    // pop rcx (Pop the right operand)
    // pop rax (Pop the left operand)
    // add rax, rcx (Add right to left, store result in rax)
    // push rax (Push the sum back to the stack)
    uint8_t code[] = {0x59, 0x58, 0x48, 0x01, 0xC8, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_sub(uint8_t **jit_ptr) {
    // pop rcx (Pop the right operand)
    // pop rax (Pop the left operand)
    // sub rax, rcx (Subtract right from left, store result in rax)
    // push rax (Push the difference back to the stack)
    uint8_t code[] = {0x59, 0x58, 0x48, 0x29, 0xC8, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_mul(uint8_t **jit_ptr) {
    // pop rcx (Pop the right operand)
    // pop rax (Pop the left operand)
    // imul rax, rcx (Signed integer multiply, store result in rax)
    // push rax (Push the product back to the stack)
    uint8_t code[] = {0x59, 0x58, 0x48, 0x0F, 0xAF, 0xC1, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_div(uint8_t **jit_ptr) {
    // pop rcx (Pop the divisor)
    // pop rax (Pop the dividend)
    // cqo (Sign-extend rax into rdx:rax to prepare the 128-bit dividend)
    // idiv rcx (Divide the 128-bit rdx:rax by rcx)
    // push rax (Save the quotient, which idiv inherently stores in rax)
    uint8_t code[] = {0x59, 0x58, 0x48, 0x99, 0x48, 0xF7, 0xF9, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

void emit_rem(uint8_t **jit_ptr) {
    // pop rcx (Pop the divisor)
    // pop rax (Pop the dividend)
    // cqo (Sign-extend rax into rdx:rax to prepare the 128-bit dividend)
    // idiv rcx (Divide the 128-bit rdx:rax by rcx)
    // push rdx (Save the remainder, which idiv inherently stores in rdx)
    uint8_t code[] = {0x59, 0x58, 0x48, 0x99, 0x48, 0xF7, 0xF9, 0x52};
    emit(jit_ptr, code, sizeof(code));
}

void emit_neg(uint8_t **jit_ptr) {
    // pop rax (Pop the single operand)
    // neg rax (Mathematically negate the value using two's complement)
    // push rax (Push the negated value back to the stack)
    uint8_t code[] = {0x58, 0x48, 0xF7, 0xD8, 0x50};   
    emit(jit_ptr, code, sizeof(code));
}

void emit_ret(uint8_t **jit_ptr) {
    // pop rax (Pop the final evaluated answer off the stack into rax)
    // ret (Return control to the caller)
    // The C compiler expects the return value of a function to reside in rax per the System V AMD64 ABI
    uint8_t code[] = {0x58, 0xC3};
    emit(jit_ptr, code, sizeof(code));
}

void* allocate_jit_memory(size_t size) {
    // Request raw memory pages from the OS. Initialized as Read/Write (no Execute) to prevent immediate security flags.
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    return ptr;
}

void make_memory_executable(void* ptr, size_t size) {
    // Lock the memory block to Read/Execute, satisfying strict W^X (Write XOR Execute) memory protection policies.
    if (mprotect(ptr, size, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect failed");
        exit(1);
    }
}

int precedence(char op) {
    if (op == '~') return 100;
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

        // --- Shunting Yard Parser Frontend ---
        int pc = 0;
        char op_stack[MAX_OP_NR];
        int op_top = -1; // Tracks the index of the top element. -1 indicates the operator stack is currently empty.
        char *curr = expr;
        uint8_t expect_val = 1;

        while (*curr && pc < MAX_OP_NR-1) {
            if (isspace(*curr)) {
                curr++; // Advance the pointer to ignore any spaces or tabs
            } 
            else if (isdigit(*curr)) {
                // Parse multi-digit numbers and convert them directly to 32-bit integers
                program[pc].op = OP_PUSH;
                program[pc].arg = strtol(curr, &curr, 10);
                pc++;
                expect_val = 0; // State update: A number was parsed, so the next valid token must be an operator 
            } 
            else if (*curr == '(') {
                op_stack[++op_top] = *curr;
                curr++;
                expect_val = 1; // State update: Entering a grouped expression, so the next valid token must be a value
            } 
            else if (*curr == ')') {
                // Unwind the operator stack until the matching open parenthesis is found
                while (op_top >= 0 && op_stack[op_top] != '(') {
                    char op = op_stack[op_top--];
                    if (op == '+') program[pc++].op = OP_ADD;
                    else if (op == '-') program[pc++].op = OP_SUB;
                    else if (op == '*') program[pc++].op = OP_MUL;
                    else if (op == '/') program[pc++].op = OP_DIV;
                    else if (op == '%') program[pc++].op = OP_REM;
                    else if (op == '~') program[pc++].op = OP_NEG;
                }
                if (op_top >= 0) op_top--; // Discard the '(' token entirely
                curr++;
                expect_val = 0; // State update: A closed parenthesis group acts as a single value, so expect an operator next
            } 
            else if (*curr == '+' || *curr == '-' || *curr == '*' || *curr == '/' || *curr == '%' || *curr == '~') {
                
                char op_char = *curr; // Isolate the operator locally so we can safely modify it if needed
                
                if(expect_val) {
                    if (op_char == '-') {
                        op_char = '~'; // Morph the binary subtraction into a unary negation symbol
                    } else if (op_char == '+') {
                        curr++;
                        continue; // Unary plus is mathematically meaningless, so we skip processing it entirely
                    } else {
                        printf("Syntax error near %c\n", op_char);
                        goto next_loop;
                    }
                } 

                uint8_t is_right_assoc = (op_char == '~');
                
                // Pre-calculate the incoming operator's precedence to optimize the loop
                int prec_op_char = precedence(op_char);
                
                // Evaluate associativity and PEMDAS precedence against the current stack top
                // Uses prec_op_char instead of *curr to properly support translated unary operators
                while (op_top >= 0 && op_stack[op_top] != '(' && precedence(op_stack[op_top]) >= prec_op_char) {

                    int prec_op_stack = precedence(op_stack[op_top]);
                    
                    if (is_right_assoc ? prec_op_stack > prec_op_char : prec_op_stack >= prec_op_char) {                  

                        char op = op_stack[op_top--];
                        if (op == '+') program[pc++].op = OP_ADD;
                        else if (op == '-') program[pc++].op = OP_SUB;
                        else if (op == '*') program[pc++].op = OP_MUL;
                        else if (op == '/') program[pc++].op = OP_DIV;
                        else if (op == '%') program[pc++].op = OP_REM;
                        else if (op == '~') program[pc++].op = OP_NEG;
                    } else {
                        break;
                    }

                }

                // Push the translated operator (op_char), NOT the raw *curr character
                op_stack[++op_top] = op_char;
                curr++;
                expect_val = 1; // State update: An operator was pushed, so the next token must be a value            
            } 
            else {
                printf("Unknown character: %c\n", *curr);
                goto next_loop;
            }
        }
        
        // Safety Catch: If the string ended but the parser was still expecting a number (e.g. "5 + ")
        if (expect_val) {
            printf("Syntax error: Incomplete expression\n");
            goto next_loop;
        }

        // Flush any remaining operators left on the stack after the input string ends
        while (op_top >= 0) {
            char op = op_stack[op_top--];
            if (op == '(') continue; // Safety catch for an unclosed parenthesis expression
            if (op == '+') program[pc++].op = OP_ADD;
            else if (op == '-') program[pc++].op = OP_SUB;
            else if (op == '*') program[pc++].op = OP_MUL;
            else if (op == '/') program[pc++].op = OP_DIV;
            else if (op == '%') program[pc++].op = OP_REM;
            else if (op == '~') program[pc++].op = OP_NEG;
        }

        program[pc].op = OP_END; // Inject the termination opcode to signal the JIT compiler to stop

        if (pc == 0) continue;

        size_t mem_size = 4096;  // Define the executable buffer size (4096 bytes = 1 standard x86-64 memory page)
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
                case OP_NEG:  emit_neg(&write_ptr); break;
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