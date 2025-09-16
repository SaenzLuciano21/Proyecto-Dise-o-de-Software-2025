%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(void);
void yyerror(const char *s);
%}

/* ---------- UNION para yylval ---------- */
%union {
    int ival;       /* literales enteros */
    char* sval;     /* identificadores */
}

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

/* ---------- PRECEDENCIAS ---------- */
%left T_OR
%left T_AND
%left T_EQ T_LT T_GT
%left T_PLUS T_MINUS
%left T_MUL T_DIV T_MOD
%right T_NOT

%%

/* ---------- GRAMÁTICA ---------- */

/* Programa principal */
programa
    : T_PROGRAM T_ID T_LBRACE declaraciones sentencias T_RBRACE
      { printf("Programa válido!\n"); }
    ;

/* Declaraciones de variables */
declaraciones
    : /* vacío */
    | declaraciones declaracion
    ;

declaracion
    : T_INTEGER T_ID T_SEMI
    | T_BOOL T_ID T_SEMI
    ;

/* Lista de sentencias */
sentencias
    : /* vacío */
    | sentencias sentencia
    ;

sentencia
    : asignacion
    | retorno
    | while_stmt
    | if_stmt
    ;

/* Asignación */
asignacion
    : T_ID T_ASSIGN expr T_SEMI
    ;

/* Return */
retorno
    : T_RETURN expr T_SEMI
    | T_RETURN T_SEMI
    ;

/* While */
while_stmt
    : T_WHILE T_LPAREN expr T_RPAREN T_LBRACE sentencias T_RBRACE
    ;

/* If */
if_stmt
    : T_IF T_LPAREN expr T_RPAREN T_THEN sentencia %prec T_THEN
    | T_IF T_LPAREN expr T_RPAREN T_THEN sentencia T_ELSE sentencia
    ;

/* Expresiones */
expr
    : T_INT_LITERAL
    | T_TRUE
    | T_FALSE
    | T_ID
    | expr T_PLUS expr
    | expr T_MINUS expr
    | expr T_MUL expr
    | expr T_DIV expr
    | expr T_MOD expr
    | expr T_LT expr
    | expr T_GT expr
    | expr T_EQ expr
    | expr T_AND expr
    | expr T_OR expr
    | T_NOT expr
    | T_LPAREN expr T_RPAREN
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error sintáctico: %s\n", s);
}

int main(int argc, char **argv) {
    extern FILE *yyin;
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror(argv[1]);
            return 1;
        }
    }
    return yyparse();
}

