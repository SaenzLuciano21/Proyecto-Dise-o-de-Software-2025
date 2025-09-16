%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc-sintaxis.tab.h"   // tokens de bison
%}

%option noyywrap

letra   [a-zA-Z]
digito  [0-9]

%%

"extern"     { return T_EXTERN; }
"bool"       { return T_BOOL; }
"program"    { return T_PROGRAM; }
"else"       { return T_ELSE; }
"then"       { return T_THEN; }
"false"      { return T_FALSE; }
"if"         { return T_IF; }
"integer"    { return T_INTEGER; }
"return"     { return T_RETURN; }
"true"       { return T_TRUE; }
"void"       { return T_VOID; }
"while"      { return T_WHILE; }

"=="         { return T_EQ; }
"&&"         { return T_AND; }
"||"         { return T_OR; }
"!"          { return T_NOT; }
"+"          { return T_PLUS; }
"-"          { return T_MINUS; }
"*"          { return T_MUL; }
"/"          { return T_DIV; }
"%"          { return T_MOD; }
"<"          { return T_LT; }
">"          { return T_GT; }
"="          { return T_ASSIGN; }

";"          { return T_SEMI; }
","          { return T_COMMA; }
"("          { return T_LPAREN; }
")"          { return T_RPAREN; }
"{"          { return T_LBRACE; }
"}"          { return T_RBRACE; }

{digito}+    { yylval.ival = atoi(yytext); return T_INT_LITERAL; }

{letra}({letra}|{digito})*  { yylval.sval = strdup(yytext); return T_ID; }

"//".*       { /* ignorar */ }
"/*"([^*]|\*+[^/])*"*/"   { /* ignorar */ }

[ \t\r\n]+   { /* ignorar */ }

.            { printf("Caracter no permitido: %s\n", yytext); }

%%

