// jit.c
#include "jit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static void emit(uint8_t **jit_ptr, const uint8_t *code, size_t len) {
    memcpy(*jit_ptr, code, len);
    *jit_ptr += len; 
}

static void emit_push(uint8_t **jit_ptr, int32_t imm4) {
    uint8_t mov_rax[] = {0x48, 0xC7, 0xC0};
    emit(jit_ptr, mov_rax, 3); 
    emit(jit_ptr, (uint8_t*)&imm4, 4); 
    uint8_t push_rax = 0x50; 
    emit(jit_ptr, &push_rax, 1); 
}

static void emit_add(uint8_t **jit_ptr) {
    uint8_t code[] = {0x59, 0x58, 0x48, 0x01, 0xC8, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

static void emit_sub(uint8_t **jit_ptr) {
    uint8_t code[] = {0x59, 0x58, 0x48, 0x29, 0xC8, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

static void emit_mul(uint8_t **jit_ptr) {
    uint8_t code[] = {0x59, 0x58, 0x48, 0x0F, 0xAF, 0xC1, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

static void emit_div(uint8_t **jit_ptr) {
    uint8_t code[] = {0x59, 0x58, 0x48, 0x99, 0x48, 0xF7, 0xF9, 0x50};
    emit(jit_ptr, code, sizeof(code));
}

static void emit_rem(uint8_t **jit_ptr) {
    uint8_t code[] = {0x59, 0x58, 0x48, 0x99, 0x48, 0xF7, 0xF9, 0x52};
    emit(jit_ptr, code, sizeof(code));
}

static void emit_neg(uint8_t **jit_ptr) {
    uint8_t code[] = {0x58, 0x48, 0xF7, 0xD8, 0x50};   
    emit(jit_ptr, code, sizeof(code));
}

static void emit_ret(uint8_t **jit_ptr) {
    uint8_t code[] = {0x58, 0xC3};
    emit(jit_ptr, code, sizeof(code));
}

static void* allocate_jit_memory(size_t size) {
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    return ptr;
}

static void make_memory_executable(void* ptr, size_t size) {
    if (mprotect(ptr, size, PROT_READ | PROT_EXEC) != 0) {
        perror("mprotect failed");
        exit(1);
    }
}

int64_t execute_jit(Instruction *program) {
    size_t mem_size = 4096;  
    uint8_t *jit_memory = (uint8_t*)allocate_jit_memory(mem_size); 
    uint8_t *write_ptr = jit_memory;

    int i = 0;
    while (1) {
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
        if (program[i].op == OP_END) break;
        i++;
    }

    make_memory_executable(jit_memory, mem_size);
    int64_t (*jit_function)(void) = (int64_t (*)(void))jit_memory;
    
    int64_t result = jit_function();
    munmap(jit_memory, mem_size);

    return result;
}
