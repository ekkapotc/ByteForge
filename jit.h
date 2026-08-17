// jit.h
#ifndef JIT_H
#define JIT_H

#include <stdint.h>
#include "vm.h"

// Compiles the VM instructions to x86-64 machine code and executes them
int64_t execute_jit(Instruction *program);

#endif // JIT_H
