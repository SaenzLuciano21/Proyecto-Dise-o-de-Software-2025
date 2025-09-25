%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "calc-sintaxis.tab.h"   // tokens de bison
%}

%option noyywrap

letra   [a-zA-Z]
digito  [0-9]

%%

"Program"            { return T_PROGRAM; }
"integer"            { return T_INTEGER; }
"bool"               { return T_BOOL; }
"void"               { return T_VOID; }
"return"             { return T_RETURN; }
"if"                 { return T_IF; }
"then"               { return T_THEN; }
"else"               { return T_ELSE; }
"while"              { return T_WHILE; }
"true"               { return T_TRUE; }
"false"              { return T_FALSE; }
"extern"             { return T_EXTERN; }

"=="                 { return T_EQ; }
"&&"                 { return T_AND; }
"||"                 { return T_OR; }
"!"                  { return T_NOT; }
"+"                  { return T_PLUS; }
"-"                  { return T_MINUS; }
"*"                  { return T_MUL; }
"/"                  { return T_DIV; }
"%"                  { return T_MOD; }
"<"                  { return T_LT; }
">"                  { return T_GT; }
"="                  { return T_ASSIGN; }
";"                  { return T_SEMI; }
","                  { return T_COMMA; }
"("                  { return T_LPAREN; }
")"                  { return T_RPAREN; }
"{"                  { return T_LBRACE; }
"}"                  { return T_RBRACE; }

{digito}+            { yylval.ival = atoi(yytext); return T_INT_LITERAL; }
{letra}({letra}|{digito}|_)*   { yylval.sval = strdup(yytext); return T_ID; }

[ \t\r\n]+           { /* ignora espacios */ }

.                    { printf("Caracter inesperado: %s\n", yytext); }

%%

