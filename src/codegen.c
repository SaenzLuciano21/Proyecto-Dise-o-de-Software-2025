#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* contadores para temporales y etiquetas */
static int temp_count = 0;
static int label_count = 0;

static char* new_temp() {
    char buf[32];
    sprintf(buf, "t%d", temp_count++);
    return strdup(buf);
}

static char* new_label() {
    char buf[32];
    sprintf(buf, "L%d", label_count++);
    return strdup(buf);
}

/* Crear TAC (duplica strings si no son NULL) */
static TAC* make_tac(const char* op, const char* a1, const char* a2, const char* res) {
    TAC* t = malloc(sizeof(TAC));
    if (!t) { perror("malloc"); exit(1); }
    t->op = op ? strdup(op) : NULL;
    t->arg1 = a1 ? strdup(a1) : NULL;
    t->arg2 = a2 ? strdup(a2) : NULL;
    t->result = res ? strdup(res) : NULL;
    t->next = NULL;
    return t;
}

/* concatena listas TAC: a seguido de b */
static TAC* join_tac(TAC* a, TAC* b) {
    if (!a) return b;
    TAC* t = a;
    while (t->next) t = t->next;
    t->next = b;
    return a;
}

/* devuelve el último nodo de la lista o NULL */
static TAC* tac_last(TAC* t) {
    if (!t) return NULL;
    while (t->next) t = t->next;
    return t;
}

/* imprime TAC en formato legible */
void print_tac(TAC* code) {
    for (TAC* t = code; t; t = t->next) {
        if (!t->op) continue;

        if (strcmp(t->op, "LABEL") == 0) {
            printf("%s:\n", t->result);
        } else if (strcmp(t->op, "GOTO") == 0) {
            printf("goto %s\n", t->result);
        } else if (strcmp(t->op, "IF_FALSE_GOTO") == 0) {
            printf("ifFalse %s goto %s\n", t->arg1 ? t->arg1 : "", t->result ? t->result : "");
        } else if (strcmp(t->op, "CALL") == 0) {
            printf("%s = call %s, %s\n",
                   t->result ? t->result : "",
                   t->arg1 ? t->arg1 : "",
                   t->arg2 ? t->arg2 : "0");
        } else if (strcmp(t->op, "RETURN") == 0) {
            if (t->arg1)
                printf("return %s\n", t->arg1);
            else
                printf("return\n");
        } else if (strcmp(t->op, "PARAM") == 0) {
            printf("param %s\n", t->arg1 ? t->arg1 : "");
        } else if (strcmp(t->op, "ASSIGN") == 0) {
            /* arg1 -> result (assignment) */
            printf("%s = %s\n",
                   t->result ? t->result : "",
                   t->arg1 ? t->arg1 : "");
        } else {
            /* default: result = arg1 op arg2, o result = arg1 si es una asignación */
            if (strcmp(t->op, "=") == 0 && t->arg1 && !t->arg2) {
                printf("%s = %s\n", t->result, t->arg1);
            } else if (t->result && t->arg2) {
                printf("%s = %s %s %s\n",
                       t->result,
                       t->arg1 ? t->arg1 : "",
                       t->op ? t->op : "",
                       t->arg2 ? t->arg2 : "");
            } else if (t->result && t->arg1 && !t->arg2) {
                printf("%s = %s %s\n",
                       t->result,
                       t->op ? t->op : "",
                       t->arg1);
            } else {
                printf("%s %s %s %s\n",
                       t->op ? t->op : "",
                       t->arg1 ? t->arg1 : "",
                       t->arg2 ? t->arg2 : "",
                       t->result ? t->result : "");
            }
        }
    }
}

/* libera lista TAC */
void free_tac(TAC* code) {
    while (code) {
        TAC* tmp = code;
        code = code->next;
        if (tmp->op) free(tmp->op);
        if (tmp->arg1) free(tmp->arg1);
        if (tmp->arg2) free(tmp->arg2);
        if (tmp->result) free(tmp->result);
        free(tmp);
    }
}

/* generación de expresiones (devuelve lista TAC cuyo último tiene campo result con la "location") */

/* gen_expr: genera TAC para expresión y devuelve lista, con última instrucción teniendo ->result */
static TAC* gen_code_internal(ASTNode* node) {
    if (!node) return NULL;

    switch (node->type) {

        case NODE_INT: {
            char tmpbuf[32];
            sprintf(tmpbuf, "%d", node->ival);
            char* t = new_temp();
            TAC* load = make_tac("=", tmpbuf, NULL, t); /* uso '=' para carga literal en temp */
            return load;
        }

        case NODE_BOOL: {
            char tmpbuf[8];
            sprintf(tmpbuf, "%d", node->ival ? 1 : 0);
            char* t = new_temp();
            TAC* load = make_tac("=", tmpbuf, NULL, t);
            return load;
        }

        case NODE_ID: {
            /* cargar el valor de la variable en un temporal */
            char* t = new_temp();
            TAC* load = make_tac("=", node->id, NULL, t); /* interpretamos "=" con arg1=var -> result=temp */
            return load;
        }

        case NODE_BINOP: {
            /* generar código para izquierda y derecha */
            TAC* c1 = gen_code_internal(node->left);
            TAC* c2 = gen_code_internal(node->right);
            char* r1 = tac_last(c1) ? tac_last(c1)->result : NULL;
            char* r2 = tac_last(c2) ? tac_last(c2)->result : NULL;
            char* tres = new_temp();
            /* op = node->op (ej. "+", "==", "&&", etc.) */
            TAC* op = make_tac(node->op, r1 ? r1 : "", r2 ? r2 : "", tres);
            return join_tac(join_tac(c1, c2), op);
        }

        case NODE_UNOP: {
            /* sólo soportamos '!' y unary '-' */
            TAC* c = gen_code_internal(node->left);
            char* r = tac_last(c) ? tac_last(c)->result : NULL;
            char* tres = new_temp();
            if (node->op && strcmp(node->op, "!") == 0) {
                TAC* op = make_tac("!", r ? r : "", NULL, tres); /* representamos unario como op '!' */
                return join_tac(c, op);
            } else if (node->op && strcmp(node->op, "-") == 0) {
                TAC* op = make_tac("NEG", r ? r : "", NULL, tres);
                return join_tac(c, op);
            } else {
                /* operador no soportado */
                return c;
            }
        }

        case NODE_FUNC_CALL: {
            /* generar args (evaluados left-to-right) */
            TAC* all = NULL;
            for (int i = 0; i < node->child_count; ++i) {
                TAC* a = gen_code_internal(node->children[i]);
                char* ar = tac_last(a) ? tac_last(a)->result : NULL;
                TAC* param = make_tac("PARAM", ar ? ar : "", NULL, NULL);
                all = join_tac(all, join_tac(a, param));
            }
            char* tres = new_temp();
            char numbuf[16];
            sprintf(numbuf, "%d", node->child_count);
            TAC* call = make_tac("CALL", node->id, numbuf, tres);
            return join_tac(all, call);
        }

        case NODE_ASSIGN: {
            /* left debe ser ID (make_id_node en tu AST) */
            /* generar RHS */
            TAC* rhs = gen_code_internal(node->right);
            char* rval = tac_last(rhs) ? tac_last(rhs)->result : NULL;
            /* instrucción: ASSIGN rval -> left->id */
            TAC* asg = make_tac("ASSIGN", rval ? rval : "", NULL, node->left->id);
            return join_tac(rhs, asg);
        }

        case NODE_RETURN: {
            if (node->left) {
                TAC* expr = gen_code_internal(node->left);
                char* r = tac_last(expr) ? tac_last(expr)->result : NULL;
                TAC* ret = make_tac("RETURN", r ? r : "", NULL, NULL);
                return join_tac(expr, ret);
            } else {
                return make_tac("RETURN", NULL, NULL, NULL);
            }
        }

        case NODE_IF: {
            /* node->left = cond, node->right = then (ASTNode), node->children[0]? = else */
            TAC* cond = gen_code_internal(node->left);
            char* cond_res = tac_last(cond) ? tac_last(cond)->result : NULL;
            char* Lelse = new_label();
            char* Lend = new_label();
            /* ifFalse cond_res goto Lelse */
            TAC* iffalse = make_tac("IF_FALSE_GOTO", cond_res ? cond_res : "", NULL, Lelse);

            TAC* then_code = gen_code_internal(node->right);
            TAC* goto_end = make_tac("GOTO", NULL, NULL, Lend);

            TAC* label_else = make_tac("LABEL", NULL, NULL, Lelse);
            TAC* label_end = make_tac("LABEL", NULL, NULL, Lend);

            TAC* seq = NULL;
            seq = join_tac(seq, cond);
            seq = join_tac(seq, iffalse);
            seq = join_tac(seq, then_code);
            seq = join_tac(seq, goto_end);

            if (node->children && node->child_count > 0 && node->children[0]) {
                /* hay else */
                TAC* else_code = gen_code_internal(node->children[0]);
                seq = join_tac(seq, label_else);
                seq = join_tac(seq, else_code);
            } else {
                /* no else: label_else será el Lend */
                seq = join_tac(seq, label_else); /* Lelse == Lend in effect; but we'll place label_end later */
            }
            seq = join_tac(seq, label_end);
            return seq;
        }

        case NODE_WHILE: {
            /* node->left = cond, node->right = body */
            char* Lstart = new_label();
            char* Lend = new_label();
            TAC* label_start = make_tac("LABEL", NULL, NULL, Lstart);
            TAC* cond = gen_code_internal(node->left);
            char* cond_res = tac_last(cond) ? tac_last(cond)->result : NULL;
            TAC* iffalse = make_tac("IF_FALSE_GOTO", cond_res ? cond_res : "", NULL, Lend);
            TAC* body = gen_code_internal(node->right);
            TAC* goto_start = make_tac("GOTO", NULL, NULL, Lstart);
            TAC* label_end = make_tac("LABEL", NULL, NULL, Lend);

            TAC* seq = NULL;
            seq = join_tac(seq, label_start);
            seq = join_tac(seq, cond);
            seq = join_tac(seq, iffalse);
            seq = join_tac(seq, body);
            seq = join_tac(seq, goto_start);
            seq = join_tac(seq, label_end);
            return seq;
        }

        case NODE_BLOCK: {
            TAC* total = NULL;
            for (int i = 0; i < node->child_count; ++i) {
                total = join_tac(total, gen_code_internal(node->children[i]));
            }
            return total;
        }

        case NODE_FUNC: {
            printf("=== Generando código para función: %s ===\n", node->id);
            print_ast(node, 1);
        
            char labelbuf[64];
            sprintf(labelbuf, "%s", node->id);
            TAC* label = make_tac("LABEL", NULL, NULL, labelbuf);
            TAC* seq = label;
        
            /* generar código del cuerpo */
            if (node->child_count > 0) {
                ASTNode* bodyNode = node->children[node->child_count - 1];
                if (bodyNode && bodyNode->type == NODE_BLOCK) {
                    for (int i = 0; i < bodyNode->child_count; ++i) {
                        TAC* stmt = gen_code_internal(bodyNode->children[i]);
                        seq = join_tac(seq, stmt);
                    }
                } else {
                    seq = join_tac(seq, gen_code_internal(bodyNode));
                }
            }
            
            return seq;
        }

        case NODE_EXTERN_FUNC: {
            /* extern function: we just emit a label (no body) */
            char labelbuf[64];
            sprintf(labelbuf, "%s", node->id);
            TAC* label = make_tac("LABEL", NULL, NULL, labelbuf);
            return label;
        }

        case NODE_PARAM: {
            /* params are handled implicitly; but emit nothing here */
            return NULL;
        }

        case NODE_PROG: {
            /* program: iterate over decls (children) */
            TAC* total = NULL;
            for (int i = 0; i < node->child_count; ++i) {
                total = join_tac(total, gen_code_internal(node->children[i]));
            }
            return total;
        }

        default:
            fprintf(stderr, "Nodo sin generación implementada: %d\n", node->type);
            return NULL;
    }
}

/* wrapper para la API del header: usamos el mismo nombre gen_code */
TAC* gen_code(ASTNode* node) {
    return gen_code_internal(node);
}

