#ifndef SYMTABLE_H
#define SYMTABLE_H
#include "ast.h"

typedef struct Symbol {
    char *name;
    VarType type;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    struct Scope *parent;
} Scope;

Scope* create_scope(Scope* parent);
void free_scope(Scope* scope);

int insert_symbol(Scope* scope, char* name, VarType type);
Symbol* lookup_symbol(Scope* scope, char* name);

#endif

