#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static ASTNode* new_node(NodeType t) {
    ASTNode* n = malloc(sizeof(ASTNode));
    n->type = t;
    n->id = NULL;
    n->op = NULL;
    n->ival = 0;
    n->left = n->right = NULL;
    n->children = NULL;
    n->child_count = 0;
    n->vtype = TYPE_VOID;
    return n;
}

ASTNode* make_int_node(int val) {
    ASTNode* n = new_node(NODE_INT);
    n->ival = val;
    return n;
}

ASTNode* make_bool_node(int val) {
    ASTNode* n = new_node(NODE_BOOL);
    n->ival = val;
    return n;
}

ASTNode* make_id_node(char* name) {
    ASTNode* n = new_node(NODE_ID);
    n->id = strdup(name);
    return n;
}

ASTNode* make_binop_node(char* op, ASTNode* l, ASTNode* r) {
    ASTNode* n = new_node(NODE_BINOP);
    n->op = strdup(op);
    n->left = l;
    n->right = r;
    return n;
}

ASTNode* make_unop_node(char* op, ASTNode* expr) {
    ASTNode* n = new_node(NODE_UNOP);
    n->op = strdup(op);
    n->left = expr;
    return n;
}

ASTNode* make_assign_node(ASTNode* id, ASTNode* expr) {
    ASTNode* n = new_node(NODE_ASSIGN);
    n->left = id;
    n->right = expr;
    return n;
}

ASTNode* make_return_node(ASTNode* expr) {
    ASTNode* n = new_node(NODE_RETURN);
    n->left = expr;
    return n;
}

ASTNode* make_if_node(ASTNode* cond, ASTNode* then_b, ASTNode* else_b) {
    ASTNode* n = new_node(NODE_IF);
    n->left = cond; // condición
    n->child_count = else_b ? 2 : 1;
    n->children = malloc(sizeof(ASTNode*) * n->child_count);
    n->children[0] = then_b;
    if (else_b) n->children[1] = else_b;
    return n;
}

ASTNode* make_while_node(ASTNode* cond, ASTNode* body) {
    ASTNode* n = new_node(NODE_WHILE);
    n->left = cond;
    n->right = body;
    return n;
}

ASTNode* make_block_node(ASTNode** stmts, int count) {
    ASTNode* n = new_node(NODE_BLOCK);
    n->child_count = count;
    if (count > 0) {
        n->children = malloc(sizeof(ASTNode*) * count);
        for (int i = 0; i < count; i++) n->children[i] = stmts[i];
    }
    return n;
}

ASTNode* make_prog_node(ASTNode** decls, int dcount, ASTNode** stmts, int scount) {
    ASTNode* n = new_node(NODE_PROG);
    n->children = malloc((dcount + scount) * sizeof(ASTNode*));
    for (int i = 0; i < dcount; i++) n->children[i] = decls[i];
    for (int j = 0; j < scount; j++) n->children[dcount + j] = stmts[j];
    n->child_count = dcount + scount;
    return n;
}

ASTNode* make_param_node(VarType tipo, char* name) {
    ASTNode* n = new_node(NODE_PARAM);
    n->id = strdup(name);
    n->vtype = tipo;
    return n;
}

ASTNode* make_func_node(VarType tipo, char* name, ASTNode** params, int param_count, ASTNode* body) {
    ASTNode* n = new_node(NODE_FUNC);
    n->id = strdup(name);
    n->vtype = tipo;
    n->children = params;
    n->child_count = param_count;
    n->left = body;  // cuerpo de la función
    return n;
}

ASTNode* make_extern_func_node(VarType tipo, char* name, ASTNode** params, int param_count) {
    ASTNode* n = new_node(NODE_EXTERN_FUNC);
    n->id = strdup(name);
    n->vtype = tipo;
    n->children = params;
    n->child_count = param_count;
    return n;
}

ASTNode* make_func_call_node(char* name, ASTNode** args, int arg_count) {
    ASTNode* n = new_node(NODE_FUNC);
    n->id = strdup(name);
    n->children = args;
    n->child_count = arg_count;
    return n;
}

/* Print AST */
void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    for (int i=0;i<indent;i++) printf("  ");

    switch (node->type) {
        case NODE_INT:   	printf("INT %d\n", node->ival); break;
        case NODE_BOOL:  	printf("BOOL %s\n", node->ival ? "true" : "false"); break;
        case NODE_ID:    	printf("ID %s\n", node->id); break;
        case NODE_BINOP: 	printf("BINOP %s\n", node->op); break;
        case NODE_UNOP:  	printf("UNOP %s\n", node->op); break;
        case NODE_ASSIGN:	printf("ASSIGN\n"); break;
        case NODE_RETURN:	printf("RETURN\n"); break;
        case NODE_FUNC:         printf("FUNC %s\n", node->id); break;
        case NODE_EXTERN_FUNC:  printf("EXTERN FUNC %s\n", node->id); break;    
        case NODE_IF:    	printf("IF\n"); break;
        case NODE_WHILE: 	printf("WHILE\n"); break;
        case NODE_BLOCK: 	printf("BLOCK\n"); break;
        case NODE_PROG:  	printf("PROGRAM\n"); break;
        case NODE_FUNC_CALL: 	printf("FUNC_CALL %s\n", node->id); break;
        default:         	printf("UNKNOWN\n");
    }

    if (node->left) print_ast(node->left, indent+1);
    if (node->right) print_ast(node->right, indent+1);
    for (int i=0; i<node->child_count; i++) {
        print_ast(node->children[i], indent+1);
    }
}

void free_ast(ASTNode* node) {
    if (!node) return;
    free(node->id);
    free(node->op);
    free_ast(node->left);
    free_ast(node->right);
    for (int i=0;i<node->child_count;i++) {
        free_ast(node->children[i]);
    }
    free(node->children);
    free(node);
}

