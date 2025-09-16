#!/bin/bash

# Salir si hay un error
set -e

# Compilar Flex y Bison
flex calc-lexico.l
bison -d calc-sintaxis.y

# Compilar con GCC
gcc -o calc lex.yy.c calc-sintaxis.tab.c -lfl

# Ejecutar tests
for file in ../tests/entrada*.c; do
    echo "Ejecutando $file..."
    ./calc "$file"
    echo "------------------------"
done

