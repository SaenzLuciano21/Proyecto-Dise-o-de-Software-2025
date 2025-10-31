#ifndef CODEGEN_H
#define CODEGEN_H
#include "ast.h"

/* ---------- Estructura del Código Intermedio (TAC) ---------- */
typedef struct TAC {
    char *op;           // operador o instrucción (ADD, SUB, IFGOTO, CALL, etc.)
    char *arg1;         // primer operando
    char *arg2;         // segundo operando
    char *result;       // resultado (temporal o variable)
    struct TAC *next;   // siguiente instrucción
} TAC;

/* ---------- Funciones ---------- */
TAC* gen_code(ASTNode* node);
void print_tac(TAC* code);
void free_tac(TAC* code);

#endif

