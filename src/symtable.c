#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"

Scope* create_scope(Scope* parent){
    Scope* s = malloc(sizeof(Scope));
    s->symbols = NULL;
    s->parent = parent;
    return s;
}

void free_scope(Scope* scope){
    Symbol* sym = scope->symbols;
    while(sym){
        Symbol* tmp = sym;
        sym = sym->next;
        free(tmp->name);
        free(tmp);
    }
    free(scope);
}

int insert_symbol(Scope* scope, char* name, VarType type){
    if(lookup_symbol(scope,name)){
        fprintf(stderr,"Error: simbolo '%s' ya declarado\n",name);
        return 0;
    }
    Symbol* s = malloc(sizeof(Symbol));
    s->name = strdup(name);
    s->type = type;
    s->next = scope->symbols;
    scope->symbols = s;
    return 1;
}

Symbol* lookup_symbol(Scope* scope, char* name){
    for(Scope* s=scope; s!=NULL; s=s->parent){
        for(Symbol* sym=s->symbols;sym;sym=sym->next){
            if(strcmp(sym->name,name)==0) return sym;
        }
    }
    return NULL;
}

