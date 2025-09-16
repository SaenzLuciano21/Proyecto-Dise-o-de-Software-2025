# Proyecto Taller de Diseño de Software
## Integrantes:

    Luciano Saenz (@SaenzLuciano21)

## Descripción

Este proyecto implementa un compilador básico en C utilizando Lex y Bison.
Incluye:

    Analizador léxico
    Analizador sintáctico

## Estructura del proyecto

├── src/
│ ├── calc-lexico.l
│ ├── calc-sintaxis.y
│ ├── ast.c
│ ├── ast.h
│ └── ...
├── tests/
│ └── entrada.c
├── README.md

## Requisitos

Instalar las siguientes dependencias:

    sudo apt install flex bison gcc

## Compilación

Se puede compilar y ejecutar usando los siguientes comandos desde la carpeta `src`:

flex calc-lexico.l
bison -d calc-sintaxis.y
gcc -o calc lex.yy.c calc-sintaxis.tab.c -lfl

o ejecutando el archivo script "Run.sh" dentro de la carpeta `src`.

## Ejecución Tests

Ejemplo de uso:

    ./calc ../tests/entrada.c
    ./calc ../tests/entrada2.c
    ./calc ../tests/entrada3.c
    ./calc ../tests/entrada4.c

