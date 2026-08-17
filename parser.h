// parser.h
#ifndef PARSER_H
#define PARSER_H

#include "vm.h"

// Parses standard math into VM bytecode. Returns 0 on success, -1 on failure.
int parse_expression(const char *expr, Instruction *program);

#endif // PARSER_H
