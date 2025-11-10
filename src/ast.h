#ifndef AST_H
#define AST_H

typedef enum {
    NODE_INT, NODE_BOOL, NODE_ID,
    NODE_BINOP, NODE_UNOP,
    NODE_ASSIGN, NODE_RETURN,
    NODE_IF, NODE_WHILE,
    NODE_BLOCK, NODE_DECL,
    NODE_PROG,
    NODE_FUNC, NODE_EXTERN_FUNC, NODE_PARAM,
    NODE_FUNC_CALL
} NodeType;

typedef enum { TYPE_INT, TYPE_BOOL, TYPE_VOID } VarType;

typedef struct ASTNode {
    NodeType type;
    char *id;           // nombre de variable o función
    int ival;           // para literales enteros o bool
    char *op;           // operador (+, -, *, ==, etc.)
    VarType vtype;      // tipo de variable o función
    struct ASTNode *left, *right;   // hijos binarios
    struct ASTNode **children;      // para listas (bloques, parámetros)
    int child_count;
} ASTNode;

/* Constructores */
ASTNode* make_int_node(int val);
ASTNode* make_bool_node(int val);
ASTNode* make_id_node(char* name);
ASTNode* make_binop_node(char* op, ASTNode* l, ASTNode* r);
ASTNode* make_unop_node(char* op, ASTNode* expr);
ASTNode* make_assign_node(ASTNode* id, ASTNode* expr);
ASTNode* make_return_node(ASTNode* expr);
ASTNode* make_if_node(ASTNode* cond, ASTNode* then_b, ASTNode* else_b);
ASTNode* make_while_node(ASTNode* cond, ASTNode* body);
ASTNode* make_block_node(ASTNode** stmts, int count);
ASTNode* make_prog_node(ASTNode** decls, int dcount, ASTNode** stmts, int scount);
ASTNode* make_func_node(VarType tipo, char* name, ASTNode** params, int param_count, ASTNode* body);
ASTNode* make_extern_func_node(VarType tipo, char* name, ASTNode** params, int param_count);
ASTNode* make_param_node(VarType tipo, char* name);
ASTNode* make_extern_func_node(VarType tipo, char* name, ASTNode** params, int param_count);
ASTNode* make_func_call_node(char* name, ASTNode** args, int arg_count);
ASTNode* fold_constants(ASTNode* node);

/* Utilidades */
void print_ast(ASTNode* node, int indent);
void free_ast(ASTNode* node);

#endif

