#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TAC* make_tac_label(char* label);
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

TAC* tac_get_last(TAC* code) {
    if (!code) return NULL;
    TAC* t = code;
    while (t->next)
        t = t->next;
    return t;
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
            printf("%s = call %s, %s\n", t->result ? t->result : "", t->arg1 ? t->arg1 : "", t->arg2 ? t->arg2 : "0");
        } else if (strcmp(t->op, "RETURN") == 0) {
            if (t->arg1)
                printf("return %s\n", t->arg1);
            else
                printf("return\n");
        } else if (strcmp(t->op, "PARAM") == 0) {
            printf("param %s\n", t->arg1 ? t->arg1 : "");
        } else if (strcmp(t->op, "ASSIGN") == 0) {
            printf("%s = %s\n", t->result ? t->result : "", t->arg1 ? t->arg1 : "");
        } else {
            if (strcmp(t->op, "=") == 0 && t->arg1 && !t->arg2) {
                printf("%s = %s\n", t->result, t->arg1);
            } else if (t->result && t->arg2) {
                printf("%s = %s %s %s\n", t->result, t->arg1 ? t->arg1 : "", t->op ? t->op : "", t->arg2 ? t->arg2 : "");
            } else if (t->result && t->arg1 && !t->arg2) {
                printf("%s = %s %s\n", t->result, t->op ? t->op : "", t->arg1);
            } else {
                printf("%s %s %s %s\n", t->op ? t->op : "", t->arg1 ? t->arg1 : "", t->arg2 ? t->arg2 : "", t->result ? t->result : "");
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
static TAC* gen_code_internal(ASTNode* node) {
    if (!node) { printf("gen_code_internal: node=NULL\n"); return NULL; }
    printf("GEN debug: node type=%d, child_count=%d, id=%s\n",
           node->type, node->child_count, node->id?node->id:"(no id)");
    if (node->type == NODE_IF) {
        printf("  IF: left=%p right=%p children0=%p\n",
               (void*)node->left, (void*)node->right,
               (node->children && node->child_count>0) ? (void*)node->children[0] : NULL);
    }
    if (node->type == NODE_FUNC) {
        printf("  FUNC: child_count=%d\n", node->child_count);
        for (int i=0;i<node->child_count;i++) {
            printf("    child[%d]=%p type=%d\n", i, (void*)node->children[i],
                   node->children[i] ? node->children[i]->type : -1);
        }
    }
    
    switch (node->type) {

        case NODE_INT: {
            char tmpbuf[32];
            sprintf(tmpbuf, "%d", node->ival);
            char* t = new_temp();
            return make_tac("=", tmpbuf, NULL, t);
        }

        case NODE_BOOL: {
            char tmpbuf[8];
            sprintf(tmpbuf, "%d", node->ival ? 1 : 0);
            char* t = new_temp();
            return make_tac("=", tmpbuf, NULL, t);
        }

        case NODE_ID: {
            char* t = new_temp();
            return make_tac("=", node->id, NULL, t);
        }

        case NODE_BINOP: {
            TAC* c1 = gen_code_internal(node->left);
            TAC* c2 = gen_code_internal(node->right);
            char* r1 = tac_last(c1) ? tac_last(c1)->result : NULL;
            char* r2 = tac_last(c2) ? tac_last(c2)->result : NULL;
            char* tres = new_temp();
            TAC* op = make_tac(node->op, r1 ? r1 : "", r2 ? r2 : "", tres);
            return join_tac(join_tac(c1, c2), op);
        }

        case NODE_UNOP: {
            TAC* c = gen_code_internal(node->left);
            char* r = tac_last(c) ? tac_last(c)->result : NULL;
            char* tres = new_temp();
            if (node->op && strcmp(node->op, "!") == 0) {
                return join_tac(c, make_tac("!", r ? r : "", NULL, tres));
            } else if (node->op && strcmp(node->op, "-") == 0) {
                return join_tac(c, make_tac("NEG", r ? r : "", NULL, tres));
            } else {
                return c;
            }
        }

        case NODE_ASSIGN: {
            TAC* rhs = gen_code_internal(node->right);
            char* rval = tac_last(rhs) ? tac_last(rhs)->result : NULL;
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
            TAC* cond = gen_code_internal(node->left);
            char* label_else = new_label();
            char* label_end = new_label();
            //TAC* code = join_tac(cond, make_tac("ifFalse", cond->result, label_else, NULL));
            TAC* last_cond = tac_get_last(cond);
            TAC* code = join_tac(cond, make_tac("ifFalse", last_cond->result, label_else, NULL));

            // THEN
            TAC* then_code = gen_code_internal(node->children[0]);
            code = join_tac(code, then_code);
            code = join_tac(code, make_tac("goto", label_end, NULL, NULL));

            // ELSE (si existe)
            code = join_tac(code, make_tac_label(label_else));
            if (node->child_count > 1 && node->children[1])
                code = join_tac(code, gen_code_internal(node->children[1]));

            code = join_tac(code, make_tac_label(label_end));
            return code;
        }

        case NODE_WHILE: {
            char* Lstart = new_label();
            char* Lend = new_label();
            TAC* label_start = make_tac("LABEL", NULL, NULL, Lstart);

            TAC* cond = gen_code_internal(node->left);
            char* cond_res = tac_last(cond) ? tac_last(cond)->result : NULL;
            TAC* iffalse = make_tac("IF_FALSE_GOTO", cond_res ? cond_res : "", NULL, Lend);

            TAC* body = gen_code_internal(node->right);
            TAC* goto_start = make_tac("GOTO", NULL, NULL, Lstart);
            TAC* label_end = make_tac("LABEL", NULL, NULL, Lend);

            TAC* seq = label_start;
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
            char labelbuf[64];
            sprintf(labelbuf, "%s", node->id);
            TAC* seq = make_tac("LABEL", NULL, NULL, labelbuf);

            /* buscar cuerpo: primer BLOCK entre children (si existe) */
            for (int i = 0; i < node->child_count; ++i) {
                ASTNode* ch = node->children[i];
                if (!ch) continue;
                /* omitir params/otros; procesar todos los nodos que no sean PARAM */
                if (ch->type == NODE_PARAM) continue;
                TAC* stm = gen_code_internal(ch);
                if (stm) seq = join_tac(seq, stm);
            }

            return seq;
        }
        
        case NODE_EXTERN_FUNC: {
            char labelbuf[64];
            sprintf(labelbuf, "%s", node->id);
            return make_tac("LABEL", NULL, NULL, labelbuf);
        }

        case NODE_PARAM:
            return NULL;

        case NODE_PROG: {
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

TAC* make_tac_label(char* label) {
    return make_tac("LABEL", NULL, NULL, label);
}

