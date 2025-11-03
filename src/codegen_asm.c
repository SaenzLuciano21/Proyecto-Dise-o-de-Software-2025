#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "codegen.h"
#include "ast.h"

/* Simple string set / list utilities */
typedef struct StrNode {
    char *s;
    struct StrNode *next;
} StrNode;

static int strnode_contains(StrNode* h, const char* s) {
    for (; h; h = h->next) if (strcmp(h->s, s) == 0) return 1;
    return 0;
}

static StrNode* strnode_add(StrNode* h, const char* s) {
    if (!s) return h;
    if (strnode_contains(h, s)) return h;
    StrNode* n = malloc(sizeof(StrNode));
    n->s = strdup(s);
    n->next = h;
    return n;
}

static void strnode_free(StrNode* h) {
    while (h) {
        StrNode* t = h; h = h->next;
        free(t->s);
        free(t);
    }
}

/* Temp map: maps names (temps, params, locals) to negative rbp offsets  */
typedef struct TempMap {
    char *name;
    int offset; // negative offset from %rbp (e.g. -4, -8)
    struct TempMap *next;
} TempMap;

static TempMap* tempmap_add(TempMap* m, const char* name, int offset) {
    TempMap* n = malloc(sizeof(TempMap));
    n->name = strdup(name);
    n->offset = offset;
    n->next = m;
    return n;
}

static int tempmap_get_offset(TempMap* m, const char* name) {
    for (TempMap* t = m; t; t = t->next) if (strcmp(t->name, name) == 0) return t->offset;
    return 0; /* caller should check existence first */
}

static void tempmap_free(TempMap* m) {
    while (m) {
        TempMap* t = m; m = m->next;
        free(t->name);
        free(t);
    }
}

/* Helpers for assembly emission */
static void emit(FILE* out, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
}

/* generate unique asm labels for short-circuit and such */
static int asm_label_count = 0;
static char* asm_new_label() {
    char buf[32];
    sprintf(buf, "ASML%d", asm_label_count++);
    return strdup(buf);
}

/* detect temporary names (t0...tn) */
static int is_temp(const char* s) {
    if (!s) return 0;
    if (s[0] != 't') return 0;
    if (!isdigit((unsigned char)s[1])) return 0;
    return 1;
}

/* detect identifier (simple heuristic) */
static int is_ident(const char* s) {
    if (!s) return 0;
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    return 1;
}

/* detect number string */
static int is_number_str(const char* s) {
    if (!s) return 0;
    int i = 0;
    if (s[0] == '-') i = 1;
    for (; s[i]; ++i) if (!isdigit((unsigned char)s[i])) return 0;
    return 1;
}

/* print movl load to %eax for operand (temp->stack, ident->global, immediate->$imm) */
static void emit_load_to_eax(FILE* out, const char* operand, TempMap* map) {
    if (!operand) { emit(out, "    movl $0, %%eax\n"); return; }
    if (is_number_str(operand)) {
        emit(out, "    movl $%s, %%eax\n", operand);
    } else if (is_temp(operand)) {
        int off = tempmap_get_offset(map, operand);
        emit(out, "    movl %d(%%rbp), %%eax\n", off);
    } else {
        emit(out, "    movl %s(%%rip), %%eax\n", operand);
    }
}

/* store %eax into dest (temp->stack or ident->global) */
static void emit_store_eax_to(FILE* out, const char* dest, TempMap* map) {
    if (!dest) return;
    if (is_temp(dest)) {
        int off = tempmap_get_offset(map, dest);
        emit(out, "    movl %%eax, %d(%%rbp)\n", off);
    } else {
        emit(out, "    movl %%eax, %s(%%rip)\n", dest);
    }
}

/* Heurística globales: cualquier ASSIGN cuyo result sea identificador (no temp) -> global */
static StrNode* collect_globals_from_tac(TAC* code) {
    StrNode* g = NULL;
    for (TAC* t = code; t; t = t->next) {
        if (!t->op) continue;
        if (strcmp(t->op, "ASSIGN") == 0) {
            if (t->result && is_ident(t->result) && !is_temp(t->result)) {
                g = strnode_add(g, t->result);
            }
        }
    }
    return g;
}

/* collect temps in region [start, region_end) */
static StrNode* collect_temps_in_region(TAC* start, TAC* region_end) {
    StrNode* temps = NULL;
    for (TAC* t = start; t && t != region_end; t = t->next) {
        if (!t->op) continue;
        if (t->arg1 && is_temp(t->arg1)) temps = strnode_add(temps, t->arg1);
        if (t->arg2 && is_temp(t->arg2)) temps = strnode_add(temps, t->arg2);
        if (t->result && is_temp(t->result)) temps = strnode_add(temps, t->result);
    }
    return temps;
}

/* find next function label (LABEL whose result does NOT start with 'L' + digit) */
static TAC* next_func_label_after(TAC* t) {
    if (!t) return NULL;
    for (TAC* u = t->next; u; u = u->next) {
        if (!u->op) continue;
        if (strcmp(u->op, "LABEL") == 0) {
            char* r = u->result;
            if (r && !(r[0]=='L' && isdigit((unsigned char)r[1]))) return u;
        }
    }
    return NULL;
}

/* find function AST node by name */
static ASTNode* find_func_node(ASTNode* root, const char* funcname) {
    if (!root) return NULL;
    if (root->type == NODE_FUNC && root->id && strcmp(root->id, funcname) == 0) return root;
    if (root->children) {
        for (int i = 0; i < root->child_count; ++i) {
            ASTNode* r = find_func_node(root->children[i], funcname);
            if (r) return r;
        }
    }
    if (root->left) {
        ASTNode* r = find_func_node(root->left, funcname);
        if (r) return r;
    }
    if (root->right) {
        ASTNode* r = find_func_node(root->right, funcname);
        if (r) return r;
    }
    return NULL;
}

/* get parameter names from AST function node (in order of children) */
static StrNode* collect_param_names(ASTNode* funcnode) {
    StrNode* p = NULL;
    if (!funcnode) return NULL;
    for (int i = 0; i < funcnode->child_count; ++i) {
        ASTNode* ch = funcnode->children[i];
        if (ch && ch->type == NODE_PARAM) {
            p = strnode_add(p, ch->id);
        }
    }
    return p;
}

/* collect local variable names from a BLOCK node (heurística: ASSIGN nodes direct children are var-decls) */
static StrNode* collect_locals_from_block(ASTNode* blocknode) {
    StrNode* locals = NULL;
    if (!blocknode || blocknode->type != NODE_BLOCK) return NULL;
    for (int i = 0; i < blocknode->child_count; ++i) {
        ASTNode* ch = blocknode->children[i];
        if (!ch) continue;
        if (ch->type == NODE_ASSIGN && ch->left && ch->left->type == NODE_ID) {
            locals = strnode_add(locals, ch->left->id);
        }
    }
    return locals;
}

/* Emit binary op (with short-circuit for && and ||)                    */
static void emit_binop(FILE* out, const char* op, const char* a1, const char* a2, const char* res, TempMap* map) {
    if (!op || !res) return;
    // load a1 into eax
    emit_load_to_eax(out, a1, map);

    if (strcmp(op, "+") == 0) {
        if (is_number_str(a2)) emit(out, "    addl $%s, %%eax\n", a2);
        else if (is_temp(a2)) emit(out, "    addl %d(%%rbp), %%eax\n", tempmap_get_offset(map, a2));
        else emit(out, "    addl %s(%%rip), %%eax\n", a2);
        emit_store_eax_to(out, res, map);
    } else if (strcmp(op, "-") == 0) {
        if (is_number_str(a2)) emit(out, "    subl $%s, %%eax\n", a2);
        else if (is_temp(a2)) emit(out, "    subl %d(%%rbp), %%eax\n", tempmap_get_offset(map, a2));
        else emit(out, "    subl %s(%%rip), %%eax\n", a2);
        emit_store_eax_to(out, res, map);
    } else if (strcmp(op, "*") == 0) {
        if (is_number_str(a2)) emit(out, "    imull $%s, %%eax\n", a2);
        else if (is_temp(a2)) emit(out, "    imull %d(%%rbp), %%eax\n", tempmap_get_offset(map, a2));
        else emit(out, "    imull %s(%%rip), %%eax\n", a2);
        emit_store_eax_to(out, res, map);
    } else if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        // divisor -> ecx
        if (is_number_str(a2)) emit(out, "    movl $%s, %%ecx\n", a2);
        else if (is_temp(a2)) emit(out, "    movl %d(%%rbp), %%ecx\n", tempmap_get_offset(map, a2));
        else emit(out, "    movl %s(%%rip), %%ecx\n", a2);
        emit(out, "    cltd\n    idivl %%ecx\n"); // quotient->eax remainder->edx
        if (strcmp(op, "/") == 0) emit_store_eax_to(out, res, map);
        else {
            emit(out, "    movl %%edx, %%eax\n");
            emit_store_eax_to(out, res, map);
        }
    } else if (strcmp(op, "==") == 0 || strcmp(op, "<") == 0 || strcmp(op, ">") == 0) {
        if (is_number_str(a2)) emit(out, "    cmpl $%s, %%eax\n", a2);
        else if (is_temp(a2)) emit(out, "    cmpl %d(%%rbp), %%eax\n", tempmap_get_offset(map, a2));
        else emit(out, "    cmpl %s(%%rip), %%eax\n", a2);
        if (strcmp(op, "==") == 0) emit(out, "    sete %%al\n");
        else if (strcmp(op, "<") == 0) emit(out, "    setl %%al\n");
        else emit(out, "    setg %%al\n");
        emit(out, "    movzbl %%al, %%eax\n");
        emit_store_eax_to(out, res, map);
    } else if (strcmp(op, "&&") == 0) {
        // short-circuit AND
        char* Lfalse = asm_new_label();
        char* Lend = asm_new_label();

        emit(out, "    cmpl $0, %%eax\n");
        emit(out, "    je %s\n", Lfalse);

        // evaluate b
        if (is_number_str(a2)) emit(out, "    movl $%s, %%eax\n", a2);
        else if (is_temp(a2)) emit(out, "    movl %d(%%rbp), %%eax\n", tempmap_get_offset(map, a2));
        else emit(out, "    movl %s(%%rip), %%eax\n", a2);

        emit(out, "    cmpl $0, %%eax\n    setne %%al\n    movzbl %%al, %%eax\n");
        emit_store_eax_to(out, res, map);
        emit(out, "    jmp %s\n", Lend);

        emit(out, "%s:\n", Lfalse);
        emit(out, "    movl $0, %%eax\n");
        emit_store_eax_to(out, res, map);

        emit(out, "%s:\n", Lend);
        free(Lfalse); free(Lend);
    } else if (strcmp(op, "||") == 0) {
        // short-circuit OR
        char* Ltrue = asm_new_label();
        char* Lend = asm_new_label();

        emit(out, "    cmpl $0, %%eax\n");
        emit(out, "    jne %s\n", Ltrue);

        // evaluate b
        if (is_number_str(a2)) emit(out, "    movl $%s, %%eax\n", a2);
        else if (is_temp(a2)) emit(out, "    movl %d(%%rbp), %%eax\n", tempmap_get_offset(map, a2));
        else emit(out, "    movl %s(%%rip), %%eax\n", a2);

        emit(out, "    cmpl $0, %%eax\n    setne %%al\n    movzbl %%al, %%eax\n");
        emit_store_eax_to(out, res, map);
        emit(out, "    jmp %s\n", Lend);

        emit(out, "%s:\n", Ltrue);
        emit(out, "    movl $1, %%eax\n");
        emit_store_eax_to(out, res, map);

        emit(out, "%s:\n", Lend);
        free(Ltrue); free(Lend);
    } else {
        // fallback: simply store a1
        emit_store_eax_to(out, res, map);
    }
}

/* Main generator: gen_asm  */
void gen_asm(TAC* code, ASTNode* ast_root, FILE* out) {
    if (!code || !out) return;

    // collect globals
    StrNode* globals = collect_globals_from_tac(code);

    // header
    emit(out, "    .text\n");
    emit(out, "    .global main\n\n");

    // data
    emit(out, "    .section .data\n");
    for (StrNode* g = globals; g; g = g->next) {
        emit(out, "%s:\n    .long 0\n", g->s);
    }
    emit(out, "\n    .section .text\n");

    // walk TAC, find function labels
    for (TAC* t = code; t; ) {
        // find next function label
        while (t && !(t->op && strcmp(t->op, "LABEL") == 0 &&
                      t->result && !(t->result[0]=='L' && isdigit((unsigned char)t->result[1]))))
            t = t->next;
        if (!t) break;

        char* funcname = t->result;
        TAC* next_func = next_func_label_after(t);

        // collect temps in region for the function (exclusive of next_func)
        StrNode* temps = collect_temps_in_region(t->next, next_func);

        // locate AST function node to get params and locals
        ASTNode* funcnode = find_func_node(ast_root, funcname);
        StrNode* params = collect_param_names(funcnode);
        StrNode* locals = NULL;
        // find body block if available (last child that is BLOCK)
        ASTNode* body = NULL;
        if (funcnode) {
            for (int i = 0; i < funcnode->child_count; ++i) {
                if (funcnode->children[i] && funcnode->children[i]->type == NODE_BLOCK) {
                    body = funcnode->children[i];
                    break;
                }
            }
            if (body) locals = collect_locals_from_block(body);
        }

        // assign offsets: param slots first, then locals, then temps
        TempMap* tmap = NULL;
        int count_params = 0; for (StrNode* s = params; s; s = s->next) ++count_params;
        int count_locals = 0; for (StrNode* s = locals; s; s = s->next) ++count_locals;
        int count_temps = 0; for (StrNode* s = temps; s; s = s->next) ++count_temps;

        // each slot 4 bytes, align to 16 bytes total
        int total_slots = count_params + count_locals + count_temps;
        int bytes_needed = total_slots * 4;
        // align to 16
        int stack_for_locals = ((bytes_needed + 15) / 16) * 16;
        if (stack_for_locals == 0) stack_for_locals = 0;

        // mapping: we will fill mapping in order: params -> locals -> temps
        int cur_off = -4;
        for (StrNode* s = params; s; s = s->next) {
            tmap = tempmap_add(tmap, s->s, cur_off);
            cur_off -= 4;
        }
        for (StrNode* s = locals; s; s = s->next) {
            tmap = tempmap_add(tmap, s->s, cur_off);
            cur_off -= 4;
        }
        for (StrNode* s = temps; s; s = s->next) {
            tmap = tempmap_add(tmap, s->s, cur_off);
            cur_off -= 4;
        }

        // emit prologue
        emit(out, "%s:\n", funcname);
        emit(out, "    pushq %%rbp\n");
        emit(out, "    movq %%rsp, %%rbp\n");
        if (stack_for_locals > 0) emit(out, "    subq $%d, %%rsp\n", stack_for_locals);

        // copy parameters from caller stack into local slots
        // caller pushed args (8 bytes each); in callee after prologue:
        // first arg is at 16(%rbp), second at 24(%rbp), ...
        if (count_params > 0) {
            int i = 0;
            int base = 16;
            for (StrNode* s = params; s; s = s->next) {
                int dest = tempmap_get_offset(tmap, s->s);
                emit(out, "    movl %d(%%rbp), %%eax\n", base + 8 * i);
                emit(out, "    movl %%eax, %d(%%rbp)\n", dest);
                ++i;
            }
        }

        // process TAC instructions in function region
        for (TAC* cur = t->next; cur && cur != next_func; cur = cur->next) {
            if (!cur->op) continue;

            if (strcmp(cur->op, "LABEL") == 0) {
                if (cur->result) emit(out, "%s:\n", cur->result);
            } else if (strcmp(cur->op, "=") == 0 && cur->result && is_temp(cur->result)) {
                // tX = literal OR tX = ident
                if (cur->arg1 && is_number_str(cur->arg1)) {
                    emit(out, "    movl $%s, %%eax\n", cur->arg1);
                } else if (cur->arg1 && is_temp(cur->arg1)) {
                    emit(out, "    movl %d(%%rbp), %%eax\n", tempmap_get_offset(tmap, cur->arg1));
                } else if (cur->arg1 && is_ident(cur->arg1)) {
                    emit(out, "    movl %s(%%rip), %%eax\n", cur->arg1);
                } else {
                    emit(out, "    movl $0, %%eax\n");
                }
                emit(out, "    movl %%eax, %d(%%rbp)\n", tempmap_get_offset(tmap, cur->result));
            } else if (strcmp(cur->op, "ASSIGN") == 0) {
                // ASSIGN arg1 -> result (result is ident or temp)
                if (cur->arg1 && is_number_str(cur->arg1)) emit(out, "    movl $%s, %%eax\n", cur->arg1);
                else if (cur->arg1 && is_temp(cur->arg1)) emit(out, "    movl %d(%%rbp), %%eax\n", tempmap_get_offset(tmap, cur->arg1));
                else if (cur->arg1 && is_ident(cur->arg1)) emit(out, "    movl %s(%%rip), %%eax\n", cur->arg1);
                else emit(out, "    movl $0, %%eax\n");

                if (cur->result && is_ident(cur->result) && !is_temp(cur->result)) {
                    emit(out, "    movl %%eax, %s(%%rip)\n", cur->result);
                } else if (cur->result && is_temp(cur->result)) {
                    emit(out, "    movl %%eax, %d(%%rbp)\n", tempmap_get_offset(tmap, cur->result));
                }
            } else if (strcmp(cur->op, "IF_FALSE_GOTO") == 0) {
                // ifFalse arg1 goto result
                if (cur->arg1 && is_temp(cur->arg1)) emit(out, "    movl %d(%%rbp), %%eax\n", tempmap_get_offset(tmap, cur->arg1));
                else if (cur->arg1 && is_number_str(cur->arg1)) emit(out, "    movl $%s, %%eax\n", cur->arg1);
                else if (cur->arg1 && is_ident(cur->arg1)) emit(out, "    movl %s(%%rip), %%eax\n", cur->arg1);
                else emit(out, "    movl $0, %%eax\n");
                emit(out, "    cmpl $0, %%eax\n");
                emit(out, "    je %s\n", cur->result ? cur->result : "L_unknown");
            } else if (strcmp(cur->op, "GOTO") == 0) {
                emit(out, "    jmp %s\n", cur->result ? cur->result : "L_unknown");
            } else if (strcmp(cur->op, "RETURN") == 0) {
                if (cur->arg1) {
                    if (is_temp(cur->arg1)) emit(out, "    movl %d(%%rbp), %%eax\n", tempmap_get_offset(tmap, cur->arg1));
                    else if (is_number_str(cur->arg1)) emit(out, "    movl $%s, %%eax\n", cur->arg1);
                    else emit(out, "    movl %s(%%rip), %%eax\n", cur->arg1);
                }
                // epilog
                if (stack_for_locals > 0) emit(out, "    addq $%d, %%rsp\n", stack_for_locals);
                emit(out, "    popq %%rbp\n");
                emit(out, "    ret\n");
            } else if (strcmp(cur->op, "PARAM") == 0) {
                // push param (caller-side), here we simply encode pushq immediate or reg value
                if (cur->arg1) {
                    if (is_number_str(cur->arg1)) emit(out, "    pushq $%s\n", cur->arg1);
                    else if (is_temp(cur->arg1)) emit(out, "    movl %d(%%rbp), %%eax\n    pushq %%rax\n", tempmap_get_offset(tmap, cur->arg1));
                    else emit(out, "    movl %s(%%rip), %%eax\n    pushq %%rax\n", cur->arg1);
                } else {
                    emit(out, "    pushq $0\n");
                }
            } else if (strcmp(cur->op, "CALL") == 0) {
                // CALL func, nargs -> result (result may be temp)
                emit(out, "    call %s\n", cur->arg1 ? cur->arg1 : "unknown_func");
                if (cur->result && is_temp(cur->result)) {
                    emit(out, "    movl %%eax, %d(%%rbp)\n", tempmap_get_offset(tmap, cur->result));
                }
                if (cur->arg2) {
                    int nargs = atoi(cur->arg2);
                    if (nargs > 0) emit(out, "    addq $%d, %%rsp\n", nargs * 8);
                }
            } else {
                // fallback: treat as binary op if possible
                if (cur->result && cur->arg1) {
                    emit_binop(out, cur->op, cur->arg1, cur->arg2, cur->result, tmap);
                }
            }
        } // end for cur

        // if no explicit return, epilog
        if (stack_for_locals > 0) emit(out, "    addq $%d, %%rsp\n", stack_for_locals);
        emit(out, "    popq %%rbp\n");
        emit(out, "    ret\n\n");

        // cleanup for this function
        strnode_free(temps);
        strnode_free(params);
        strnode_free(locals);
        tempmap_free(tmap);

        // advance t to next function label
        t = next_func ? next_func : NULL;
    } // end for functions

    strnode_free(globals);
}

