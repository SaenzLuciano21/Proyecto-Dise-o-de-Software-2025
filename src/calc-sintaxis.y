%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtable.h"
#include "codegen.h"

int yylex(void);
void yyerror(const char *s);

Scope* current_scope = NULL;
ASTNode* root_ast = NULL;

/* --- Estructura para guardar firmas de funciones (no modifica symtable) --- */
typedef struct FuncInfo {
    char *name;
    VarType ret_type;
    VarType *param_types; /* array */
    int param_count;
    struct FuncInfo *next;
} FuncInfo;

FuncInfo *func_list = NULL;
VarType current_function_return_type = TYPE_VOID; /* usado para chequeo de return */

/* Prototipos */
void register_function_signature(char* name, VarType ret, VarType *param_types, int param_count);
FuncInfo* find_function(char* name);
VarType get_expr_type(ASTNode* node, Scope* scope);

/* Helpers */
void push_scope() {
    current_scope = create_scope(current_scope);
}
void pop_scope() {
    Scope* parent = current_scope->parent;
    free_scope(current_scope);
    current_scope = parent;
}

%}
%debug

/* ---------- UNION ---------- */
%union {
    int ival;
    char* sval;
    struct ASTNode* node;
    VarType tipo;
}

/* ---------- TIPOS ---------- */
%type <node> programa
%type <node> decl_vars var_decl
%type <node> bloque
%type <node> sentencias
%type <node> sentencia
%type <node> decls decl
%type <node> asignacion retorno while_stmt if_stmt
%type <node> expr expr_list
%type <node> lista_param param
%type <node> method_call
%type <tipo> tipo

/* ---------- TOKENS ---------- */
%token T_EXTERN T_BOOL T_PROGRAM T_ELSE T_THEN T_FALSE T_IF T_INTEGER
%token T_RETURN T_TRUE T_VOID T_WHILE
%token T_EQ T_AND T_OR T_NOT
%token T_PLUS T_MINUS T_MUL T_DIV T_MOD
%token T_LT T_GT T_ASSIGN
%token T_SEMI T_COMMA T_LPAREN T_RPAREN T_LBRACE T_RBRACE
%token <ival> T_INT_LITERAL
%token <sval> T_ID

%nonassoc T_THEN
%nonassoc T_ELSE

%left T_OR
%left T_AND
%left T_EQ T_LT T_GT
%left T_PLUS T_MINUS
%left T_MUL T_DIV T_MOD
%right T_NOT

%%
    
programa
  : T_PROGRAM T_LBRACE decls T_RBRACE
    {
      $$ = make_prog_node(
          $3 ? $3->children : NULL,
          $3 ? $3->child_count : 0, NULL, 0);
	  /*print_ast($$, 0); Imprime el árbol */
      printf("\n=== AST ORIGINAL ===\n");
      print_ast($$, 0);
      	
      printf("\n=== APLICANDO OPTIMIZACIÓN: PROPAGACIÓN DE CONSTANTES ===\n");
      fold_constants($$);
      
      printf("\n=== AST OPTIMIZADO ===\n");
      print_ast($$, 0);
      	
      root_ast = $$;
    }
  ;
  
decls
    : /* vacío */ { $$ = NULL; }
    | decls decl
      {
          if ($1 == NULL) {
              ASTNode* arr[1] = { $2 };
              $$ = make_block_node(arr, 1);
          } else {
              $1->children = realloc($1->children,
                                     sizeof(ASTNode*) * ($1->child_count + 1));
              $1->children[$1->child_count++] = $2;
              $$ = $1;
          }
      }
    ;

decl
    : var_decl
    | tipo T_ID T_LPAREN lista_param T_RPAREN bloque
        {
            /* registrar firma de función */
            int pcount = $4 ? $4->child_count : 0;
            VarType *ptypes = NULL;
            if (pcount > 0) {
                ptypes = malloc(sizeof(VarType) * pcount);
                for (int i=0;i<pcount;i++) ptypes[i] = $4->children[i]->vtype;
            }
            register_function_signature($2, $1, ptypes, pcount);

            /* crear scope local para la función (su cuerpo ya se construyó en $6) */
            $$ = make_func_node($1, $2, $4 ? $4->children : NULL,
                                  $4 ? $4->child_count : 0, $6);
        }
    | T_VOID T_ID T_LPAREN lista_param T_RPAREN bloque
        {
            int pcount = $4 ? $4->child_count : 0;
            VarType *ptypes = NULL;
            if (pcount > 0) {
                ptypes = malloc(sizeof(VarType) * pcount);
                for (int i=0;i<pcount;i++) ptypes[i] = $4->children[i]->vtype;
            }
            register_function_signature($2, TYPE_VOID, ptypes, pcount);

            $$ = make_func_node(TYPE_VOID, $2, $4 ? $4->children : NULL,
                                  $4 ? $4->child_count : 0, $6);
        }
    | tipo T_ID T_LPAREN lista_param T_RPAREN T_EXTERN T_SEMI
        {
            int pcount = $4 ? $4->child_count : 0;
            VarType *ptypes = NULL;
            if (pcount > 0) {
                ptypes = malloc(sizeof(VarType) * pcount);
                for (int i=0;i<pcount;i++) ptypes[i] = $4->children[i]->vtype;
            }
            register_function_signature($2, $1, ptypes, pcount);
            $$ = make_extern_func_node($1, $2, $4 ? $4->children : NULL,
                                         $4 ? $4->child_count : 0);
        }
    | T_VOID T_ID T_LPAREN lista_param T_RPAREN T_EXTERN T_SEMI
        {
            int pcount = $4 ? $4->child_count : 0;
            VarType *ptypes = NULL;
            if (pcount > 0) {
                ptypes = malloc(sizeof(VarType) * pcount);
                for (int i=0;i<pcount;i++) ptypes[i] = $4->children[i]->vtype;
            }
            register_function_signature($2, TYPE_VOID, ptypes, pcount);
            $$ = make_extern_func_node(TYPE_VOID, $2, $4 ? $4->children : NULL,
                                         $4 ? $4->child_count : 0);
        }
    ;

decl_vars
    : /* vacío */ { $$ = NULL; }
    | decl_vars var_decl
      {
          if ($1 == NULL) {
              ASTNode* arr[1] = { $2 };
              $$ = make_block_node(arr, 1);
          } else {
              $1->children = realloc($1->children, sizeof(ASTNode*) * ($1->child_count + 1));
              $1->children[$1->child_count++] = $2;
              $$ = $1;
          }
      }
    ;

var_decl
  : tipo T_ID T_ASSIGN expr T_SEMI
    {
      /* insertar variable en scope actual */
      if (!insert_symbol(current_scope, $2, $1)) {
          /* insert_symbol ya imprime error si se repite */
      } else {
          /* crear nodo de asignación (inicialización) */
          $$ = make_assign_node(make_id_node($2), $4);
      }
    }
  ;

bloque
    : T_LBRACE decl_vars sentencias T_RBRACE
      {
          int total = ($2 ? $2->child_count : 0) + ($3 ? $3->child_count : 0);
          ASTNode** all_nodes = malloc(sizeof(ASTNode*) * total);
          int idx = 0;

          if ($2)
              for (int i = 0; i < $2->child_count; i++)
                  all_nodes[idx++] = $2->children[i];
          if ($3)
              for (int i = 0; i < $3->child_count; i++)
                  all_nodes[idx++] = $3->children[i];

          $$ = make_block_node(all_nodes, idx);
      }
    ;
    
sentencias
    : /* vacío */ { $$ = NULL; }
    | sentencias sentencia
      {
          if ($1 == NULL) {
              ASTNode* arr[1] = { $2 };
              $$ = make_block_node(arr, 1);
          } else {
              $1->children = realloc($1->children,
                                     sizeof(ASTNode*) * ($1->child_count + 1));
              $1->children[$1->child_count++] = $2;
              $$ = $1;
          }
      }
    ;

method_call
    : T_ID T_LPAREN expr_list T_RPAREN
      {
          /* chequeo: existe la función y tipos/argc */
          FuncInfo* f = find_function($1);
          if (!f) {
              fprintf(stderr, "Error: función '%s' no declarada\n", $1);
              $$ = make_func_call_node($1, $3 ? $3->children : NULL, $3 ? $3->child_count : 0);
          } else {
              int argc = $3 ? $3->child_count : 0;
              if (argc != f->param_count) {
                  fprintf(stderr, "Error: llamada a '%s' con %d args, esperaba %d\n",
                          $1, argc, f->param_count);
              } else {
                  /* verificar tipos de cada argumento */
                  for (int i=0;i<argc;i++) {
                      VarType at = get_expr_type($3->children[i], current_scope);
                      if (at != f->param_types[i]) {
                          fprintf(stderr, "Error: en llamada a '%s' argumento %d tipo incompatible\n",
                                  $1, i+1);
                      }
                  }
              }
              $$ = make_func_call_node($1, $3 ? $3->children : NULL, $3 ? $3->child_count : 0);
              $$->vtype = f->ret_type;
          }
      }
    ;

sentencia
    : asignacion { $$ = $1; }
    | retorno    { $$ = $1; }
    | while_stmt { $$ = $1; }
    | if_stmt    { $$ = $1; }
    | method_call T_SEMI    { $$ = make_block_node((ASTNode*[]) { $1 }, 1); }
    | bloque     { $$ = $1; }
    | T_SEMI     { $$ = NULL; }
    ;
    
asignacion
    : T_ID T_ASSIGN expr T_SEMI
      {
          Symbol* s = lookup_symbol(current_scope, $1);
          if (!s) {
              fprintf(stderr, "Error: identificador '%s' no declarado\n", $1);
          } else {
              VarType left_t = s->type;
              VarType right_t = get_expr_type($3, current_scope);
              if (left_t != right_t)
                  fprintf(stderr, "Error: tipo incompatible en asignación a '%s'\n", $1);
          }
          $$ = make_assign_node(make_id_node($1), $3);
      }
    ;

retorno
    : T_RETURN expr T_SEMI
      {
          VarType t = get_expr_type($2, current_scope);
          if (current_function_return_type == TYPE_VOID) {
              fprintf(stderr, "Error: return con expresión en función void\n");
          } else if (t != current_function_return_type) {
              fprintf(stderr, "Error: tipo en return (%d) no coincide con tipo de función (%d)\n",
                      t, current_function_return_type);
          }
          $$ = make_return_node($2);
      }
    | T_RETURN T_SEMI
      {
          if (current_function_return_type != TYPE_VOID) {
              fprintf(stderr, "Error: return sin expresión en función que retorna valor\n");
          }
          $$ = make_return_node(NULL);
      }
    ;

if_stmt
    : T_IF T_LPAREN expr T_RPAREN T_THEN bloque %prec T_THEN
      {
          if (get_expr_type($3, current_scope) != TYPE_BOOL)
              fprintf(stderr, "Error: condición del 'if' debe ser booleana\n");
          $$ = make_if_node($3, $6, NULL);
      }
    | T_IF T_LPAREN expr T_RPAREN T_THEN bloque T_ELSE bloque
      {
          if (get_expr_type($3, current_scope) != TYPE_BOOL)
              fprintf(stderr, "Error: condición del 'if' debe ser booleana\n");
          $$ = make_if_node($3, $6, $8);
      }
    ;

while_stmt
    : T_WHILE T_LPAREN expr T_RPAREN bloque
      {
          if (get_expr_type($3, current_scope) != TYPE_BOOL)
              fprintf(stderr, "Error: condición del 'while' debe ser booleana\n");
          $$ = make_while_node($3, $5);
      }
    ;

expr
    : T_INT_LITERAL       { $$ = make_int_node($1); $$->vtype = TYPE_INT; }
    | T_TRUE              { $$ = make_bool_node(1); $$->vtype = TYPE_BOOL; }
    | T_FALSE             { $$ = make_bool_node(0); $$->vtype = TYPE_BOOL; }
    | T_ID
      {
          Symbol* sym = lookup_symbol(current_scope, $1);
          if (!sym) {
              fprintf(stderr, "Error: identificador '%s' no declarado\n", $1);
              $$ = make_id_node($1);
          } else {
              $$ = make_id_node($1);
              $$->vtype = sym->type;
          }
      }
    | expr T_PLUS expr    { $$ = make_binop_node("+", $1, $3); $$->vtype = TYPE_INT; }
    | expr T_MINUS expr   { $$ = make_binop_node("-", $1, $3); $$->vtype = TYPE_INT; }
    | expr T_MUL expr     { $$ = make_binop_node("*", $1, $3); $$->vtype = TYPE_INT; }
    | expr T_DIV expr     { $$ = make_binop_node("/", $1, $3); $$->vtype = TYPE_INT; }
    | expr T_MOD expr     { $$ = make_binop_node("%", $1, $3); $$->vtype = TYPE_INT; }
    | expr T_LT expr      { $$ = make_binop_node("<", $1, $3); $$->vtype = TYPE_BOOL; }
    | expr T_GT expr      { $$ = make_binop_node(">", $1, $3); $$->vtype = TYPE_BOOL; }
    | expr T_EQ expr      { $$ = make_binop_node("==", $1, $3); $$->vtype = TYPE_BOOL; }
    | expr T_AND expr     { $$ = make_binop_node("&&", $1, $3); $$->vtype = TYPE_BOOL; }
    | expr T_OR expr      { $$ = make_binop_node("||", $1, $3); $$->vtype = TYPE_BOOL; }
    | T_NOT expr          { $$ = make_unop_node("!", $2); $$->vtype = TYPE_BOOL; }
    | method_call         { $$ = $1; }
    ;

expr_list
    : /* vacío */ { $$ = NULL; }
    | expr { ASTNode* arr[1]={ $1 }; $$ = make_block_node(arr,1); }
    | expr_list ',' expr
      {
          $1->children = realloc($1->children, sizeof(ASTNode*)*($1->child_count+1));
          $1->children[$1->child_count++] = $3;
          $$ = $1;
      }
    ;

tipo
    : T_INTEGER { $$ = TYPE_INT; }
    | T_BOOL    { $$ = TYPE_BOOL; }
    | T_VOID    { $$ = TYPE_VOID; }
    ;

lista_param
    : /* vacío */ { $$ = NULL; }
    | param { ASTNode* arr[1]={ $1 }; $$ = make_block_node(arr,1); }
    | lista_param ',' param
      {
          $1->children = realloc($1->children,sizeof(ASTNode*)*($1->child_count+1));
          $1->children[$1->child_count++]=$3;
          $$=$1;
      }
    ;

param
    : tipo T_ID
      {
          /* crear nodo de parámetro */
          $$ = make_param_node($1,$2);
          $$->vtype = $1;
      }
    ;

%%

/* Registrar firma de función en la lista global */
void register_function_signature(char* name, VarType ret, VarType *param_types, int param_count) {
    FuncInfo* existing = find_function(name);
    if (existing) {
        fprintf(stderr, "Error: función '%s' ya declarada\n", name);
        return;
    }
    FuncInfo* f = malloc(sizeof(FuncInfo));
    f->name = strdup(name);
    f->ret_type = ret;
    f->param_count = param_count;
    f->param_types = param_types;
    f->next = func_list;
    func_list = f;
}

/* Buscar función por nombre */
FuncInfo* find_function(char* name) {
    for (FuncInfo* f = func_list; f != NULL; f = f->next) {
        if (strcmp(f->name, name) == 0) return f;
    }
    return NULL;
}

/* Obtener tipo de una expresión a partir del AST (recursivo) */
VarType get_expr_type(ASTNode* node, Scope* scope) {
    if (!node) return TYPE_VOID;

    switch (node->type) {
        case NODE_INT:
            return TYPE_INT;
        case NODE_BOOL:
            return TYPE_BOOL;
        case NODE_ID: {
            Symbol* s = lookup_symbol(scope, node->id);
            if (!s) {
                fprintf(stderr, "Error: identificador '%s' no declarado\n", node->id);
                return TYPE_VOID;
            }
            return s->type;
        }
        case NODE_BINOP: {
            VarType l = get_expr_type(node->left, scope);
            VarType r = get_expr_type(node->right, scope);

            // Operadores aritméticos → ambos integer
            if (!strcmp(node->op, "+") || !strcmp(node->op, "-") ||
                !strcmp(node->op, "*") || !strcmp(node->op, "/") || !strcmp(node->op, "%")) {
                if (l != TYPE_INT || r != TYPE_INT)
                    fprintf(stderr, "Error: operador '%s' requiere operandos integer\n", node->op);
                return TYPE_INT;
            }

            // Operadores relacionales → ambos integer, resultado bool
            if (!strcmp(node->op, "<") || !strcmp(node->op, ">")) {
                if (l != TYPE_INT || r != TYPE_INT)
                    fprintf(stderr, "Error: comparación '%s' requiere operandos integer\n", node->op);
                return TYPE_BOOL;
            }

            // Igualdad → operandos del mismo tipo
            if (!strcmp(node->op, "==")) {
                if (l != r)
                    fprintf(stderr, "Error: comparación '==' entre tipos distintos\n");
                return TYPE_BOOL;
            }

            // Lógicos → operandos booleanos
            if (!strcmp(node->op, "&&") || !strcmp(node->op, "||")) {
                if (l != TYPE_BOOL || r != TYPE_BOOL)
                    fprintf(stderr, "Error: operador lógico '%s' requiere operandos bool\n", node->op);
                return TYPE_BOOL;
            }

            return TYPE_VOID;
        }
        case NODE_UNOP:
            if (!strcmp(node->op, "!")) {
                VarType t = get_expr_type(node->left, scope);
                if (t != TYPE_BOOL)
                    fprintf(stderr, "Error: operador '!' requiere operando bool\n");
                return TYPE_BOOL;
            }
            if (!strcmp(node->op, "-")) {
                VarType t = get_expr_type(node->left, scope);
                if (t != TYPE_INT)
                    fprintf(stderr, "Error: operador '-' unario requiere integer\n");
                return TYPE_INT;
            }
            return TYPE_VOID;
        case NODE_FUNC_CALL:
            return TYPE_INT; // valor simbólico por ahora
        default:
            return TYPE_VOID;
    }
}

void yyerror(const char *s) {
    fprintf(stderr, "Error sintáctico: %s\n", s);
}

int main(int argc, char **argv) {
    extern FILE *yyin;
    /*yydebug = 1; Debug de Bison*/
    current_scope = create_scope(NULL);  // scope raíz del programa

    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror(argv[1]);
            return 1;
        }
    }
    int result = yyparse();
    
    /* --- Generar código intermedio --- */
    if (root_ast) {
    	printf("\n=== GENERACIÓN DE CÓDIGO INTERMEDIO ===\n");
    	TAC* code = gen_code(root_ast);
    	FILE* fout = fopen("out.s", "w");
    	gen_asm(code, root_ast, fout);
	fclose(fout);
    	/*print_tac(code); Imprime el código intermedio*/
    	free_tac(code);
    } else {
    	printf("No se generó AST raíz.\n");
    }
    
    free_scope(current_scope); // libera el scope raíz

    /* liberar lista de funciones */
    FuncInfo* f = func_list;
    while (f) {
        FuncInfo* tmp = f;
        f = f->next;
        free(tmp->name);
        free(tmp->param_types);
        free(tmp);
    }

    return result;
}

