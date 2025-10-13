%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtable.h"

int yylex(void);
void yyerror(const char *s);

Scope* current_scope = NULL;
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
/*%type <node> decl_funcs method_decl*/
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
          $3 ? $3->child_count : 0,
          NULL, 0);
      print_ast($$, 0);
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
        { $$ = make_func_node($1, $2, $4 ? $4->children : NULL,
                              $4 ? $4->child_count : 0, $6); }
    | T_VOID T_ID T_LPAREN lista_param T_RPAREN bloque
        { $$ = make_func_node(TYPE_VOID, $2, $4 ? $4->children : NULL,
                              $4 ? $4->child_count : 0, $6); }
    | tipo T_ID T_LPAREN lista_param T_RPAREN T_EXTERN T_SEMI
        { $$ = make_extern_func_node($1, $2, $4 ? $4->children : NULL,
                                     $4 ? $4->child_count : 0); }
    | T_VOID T_ID T_LPAREN lista_param T_RPAREN T_EXTERN T_SEMI
        { $$ = make_extern_func_node(TYPE_VOID, $2, $4 ? $4->children : NULL,
                                     $4 ? $4->child_count : 0); }
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
    { insert_symbol(current_scope, $2, $1); $$ = make_assign_node(make_id_node($2), $4); }
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

/*decl_funcs
    : { $$ = NULL; }
    | decl_funcs method_decl
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
    ;*/
    
/*method_decl
  : tipo T_ID T_LPAREN lista_param T_RPAREN bloque
      { $$ = make_func_node($1, $2, $4 ? $4->children : NULL, $4 ? $4->child_count : 0, $6); }
  | T_VOID T_ID T_LPAREN lista_param T_RPAREN bloque
      { $$ = make_func_node(TYPE_VOID, $2, $4 ? $4->children : NULL, $4 ? $4->child_count : 0, $6); }
  | tipo T_ID T_LPAREN lista_param T_RPAREN T_EXTERN T_SEMI
      { $$ = make_extern_func_node($1, $2, $4 ? $4->children : NULL, $4 ? $4->child_count : 0); }
  | T_VOID T_ID T_LPAREN lista_param T_RPAREN T_EXTERN T_SEMI
      { $$ = make_extern_func_node(TYPE_VOID, $2, $4 ? $4->children : NULL, $4 ? $4->child_count : 0); }
  ;*/

method_call
    : T_ID T_LPAREN expr_list T_RPAREN
      { $$ = make_func_call_node($1, $3 ? $3->children : NULL, $3 ? $3->child_count : 0); }
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
          if (!lookup_symbol(current_scope, $1)) {
              fprintf(stderr, "Error: identificador '%s' no declarado\n", $1);
          }
          $$ = make_assign_node(make_id_node($1), $3);
      }
    ;

retorno
    : T_RETURN expr T_SEMI { $$ = make_return_node($2); }
    | T_RETURN T_SEMI      { $$ = make_return_node(NULL); }
    ;

while_stmt
    : T_WHILE T_LPAREN expr T_RPAREN bloque
      { $$ = make_while_node($3, $5); }
    ;

if_stmt
    : T_IF T_LPAREN expr T_RPAREN T_THEN bloque %prec T_THEN
      { $$ = make_if_node($3, $6, NULL); }
    | T_IF T_LPAREN expr T_RPAREN T_THEN bloque T_ELSE bloque
      { $$ = make_if_node($3, $6, $8); }
    ;

expr
    : T_INT_LITERAL       { $$ = make_int_node($1); }
    | T_TRUE              { $$ = make_bool_node(1); }
    | T_FALSE             { $$ = make_bool_node(0); }
    | T_ID
      {
          if (!lookup_symbol(current_scope, $1)) {
              fprintf(stderr, "Error: identificador '%s' no declarado\n", $1);
          }
          $$ = make_id_node($1);
      }
    | expr T_PLUS expr    { $$ = make_binop_node("+", $1, $3); }
    | expr T_MINUS expr   { $$ = make_binop_node("-", $1, $3); }
    | expr T_MUL expr     { $$ = make_binop_node("*", $1, $3); }
    | expr T_DIV expr     { $$ = make_binop_node("/", $1, $3); }
    | expr T_MOD expr     { $$ = make_binop_node("%", $1, $3); }
    | expr T_LT expr      { $$ = make_binop_node("<", $1, $3); }
    | expr T_GT expr      { $$ = make_binop_node(">", $1, $3); }
    | expr T_EQ expr      { $$ = make_binop_node("==", $1, $3); }
    | expr T_AND expr     { $$ = make_binop_node("&&", $1, $3); }
    | expr T_OR expr      { $$ = make_binop_node("||", $1, $3); }
    | T_NOT expr          { $$ = make_unop_node("!", $2); }
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
    : tipo T_ID { $$ = make_param_node($1,$2); insert_symbol(current_scope,$2,$1); }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error sintáctico: %s\n", s);
}

int main(int argc, char **argv) {
    extern FILE *yyin;
    yydebug = 1;
    current_scope = create_scope(NULL);  // scope raíz del programa

    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror(argv[1]);
            return 1;
        }
    }
    int result = yyparse();

    free_scope(current_scope); // libera el scope raíz
    return result;
}

