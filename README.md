# Proyecto Taller de Diseño de Software
## Integrantes:

    Luciano Saenz (@SaenzLuciano21)

## Descripción

Este proyecto implementa un compilador básico en C utilizando Lex y Bison.
Incluye:

    Analizador léxico
    Analizador sintáctico
    Analizador Semántico (AST y TS)
    Generador Codigo Intermedio
    Generador Codigo Objeto

## Estructura del proyecto

├── src/
│ ├── calc-lexico.l
│ ├── calc-sintaxis.y
│ ├── ast.c
│ ├── ast.h
│ └── codegen.c
│ └── codegen.h
│ └── codegen_asm.c
│ └── symtable.c
│ └── symtable.h
├── tests/
│ └── validos
│    ├── entrada.c
│    ├── entrada5.c
| └── invalidos
│    ├── entrada2.c
│    ├── entrada3.c
│    ├── entrada4.c
├── README.md

## Requisitos

Instalar las siguientes dependencias:

    sudo apt install flex bison gcc

## Compilación

Se puede compilar y ejecutar usando los siguientes comandos desde la carpeta `src`:

flex calc-lexico.l
bison -d calc-sintaxis.y
gcc -o calc calc-sintaxis.tab.c lex.yy.c ast.c symtable.c codegen.c codegen_asm.c -lfl
o ejecutando el archivo script "Run.sh" dentro de la carpeta `src`.

## Ejecución Tests

Ejemplo de uso:

    ./calc ../tests/validos/entrada.c
    ./calc ../tests/invalidos/entrada2.c
    ./calc ../tests/invalidos/entrada3.c
    ./calc ../tests/invalidos/entrada4.c
    ./calc ../tests/validos/entrada5.c
