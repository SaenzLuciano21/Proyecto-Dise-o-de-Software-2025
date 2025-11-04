#!/bin/bash

# Salir si hay un error
set -e

# Compilar Flex y Bison
flex calc-lexico.l
bison -d calc-sintaxis.y

# Compilar con GCC
gcc -o calc calc-sintaxis.tab.c lex.yy.c ast.c symtable.c codegen.c codegen_asm.c -lfl


# Ejecutar tests
for file in ../tests/validos/entrada*.c; do
    echo "Ejecutando Tests VALIDOS $file..."
    ./calc "$file"
    echo "------------------------"
done

for file in ../tests/invalidos/entrada*.c; do
    echo "Ejecutando Tests INVALIDOS $file..."
    ./calc "$file"
    echo "------------------------"
done
