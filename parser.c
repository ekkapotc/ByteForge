// parser.c
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static int precedence(char op) {
    if (op == '~') return 100;
    if (op == '%') return 4;
    if (op == '/') return 3;
    if (op == '*') return 2;
    if (op == '+' || op == '-') return 1;
    return 0;
}

int parse_expression(const char *expr, Instruction *program) {
    int pc = 0;
    char op_stack[MAX_OP_NR];
    int op_top = -1; 
    const char *curr = expr;
    uint8_t expect_val = 1;

    while (*curr && pc < MAX_OP_NR-1) {
        if (isspace(*curr)) {
            curr++; 
        } 
        else if (isdigit(*curr)) {
            program[pc].op = OP_PUSH;
            program[pc].arg = strtol(curr, (char**)&curr, 10);
            pc++;
            expect_val = 0; 
        } 
        else if (*curr == '(') {
            op_stack[++op_top] = *curr;
            curr++;
            expect_val = 1; 
        } 
        else if (*curr == ')') {
            while (op_top >= 0 && op_stack[op_top] != '(') {
                char op = op_stack[op_top--];
                if (op == '+') program[pc++].op = OP_ADD;
                else if (op == '-') program[pc++].op = OP_SUB;
                else if (op == '*') program[pc++].op = OP_MUL;
                else if (op == '/') program[pc++].op = OP_DIV;
                else if (op == '%') program[pc++].op = OP_REM;
                else if (op == '~') program[pc++].op = OP_NEG;
            }
            if (op_top >= 0) op_top--; 
            curr++;
            expect_val = 0; 
        } 
        else if (*curr == '+' || *curr == '-' || *curr == '*' || *curr == '/' || *curr == '%' || *curr == '~') {
            
            char op_char = *curr; 
            
            if(expect_val) {
                if (op_char == '-') {
                    op_char = '~'; 
                } else if (op_char == '+') {
                    curr++;
                    continue; 
                } else {
                    printf("Syntax error near %c\n", op_char);
                    return -1;
                }
            } 

            uint8_t is_right_assoc = (op_char == '~');
            int prec_op_char = precedence(op_char);
            
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

            op_stack[++op_top] = op_char;
            curr++;
            expect_val = 1;         
        } 
        else {
            printf("Unknown character: %c\n", *curr);
            return -1;
        }
    }
    
    if (expect_val) {
        printf("Syntax error: Incomplete expression\n");
        return -1;
    }

    while (op_top >= 0) {
        char op = op_stack[op_top--];
        if (op == '(') continue; 
        if (op == '+') program[pc++].op = OP_ADD;
        else if (op == '-') program[pc++].op = OP_SUB;
        else if (op == '*') program[pc++].op = OP_MUL;
        else if (op == '/') program[pc++].op = OP_DIV;
        else if (op == '%') program[pc++].op = OP_REM;
        else if (op == '~') program[pc++].op = OP_NEG;
    }

    program[pc].op = OP_END; 
    return 0;
}
