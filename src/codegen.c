#include "srclang.h"

char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
char *argreg2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int labelseq = 1;
int brkseq;
int contseq;
char *funcname;

extern FILE *cout;

void gen(Node *node);

void gen_addr(Node *node) {
    switch (node->kind) {
        case ND_VAR: {
            Var *var = node->var;
            if (var->is_local) {
                fprintf(cout, "  lea rax, [rbp-%d]\n", var->offset);
                fprintf(cout, "  push rax\n");
            } else {
                fprintf(cout, "  push offset %s\n", var->name);
            }
            return;
        }
        case ND_DEREF:
            gen(node->lhs);
            return;
        case ND_MEMBER:
            gen_addr(node->lhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  add rax, %d\n", node->member->offset);
            fprintf(cout, "  push rax\n");
            return;
    }

    error_tok(node->tok, "not an lvalue");
}

void gen_lval(Node *node) {
    if (node->ty->kind == TY_ARRAY)
        error_tok(node->tok, "not an lvalue");
    gen_addr(node);
}

void load(Type *ty) {
    fprintf(cout, "  pop rax\n");

    int sz = size_of(ty, NULL);
    if (sz == 1) {
        fprintf(cout, "  movsx rax, byte ptr [rax]\n");
    } else if (sz == 2) {
        fprintf(cout, "  movsx rax, word ptr [rax]\n");
    } else if (sz == 4) {
        fprintf(cout, "  movsxd rax, dword ptr [rax]\n");
    } else {
        assert(sz == 8);
        fprintf(cout, "  mov rax, [rax]\n");
    }

    fprintf(cout, "  push rax\n");
}

void store(Type *ty) {
    fprintf(cout, "  pop rdi\n");
    fprintf(cout, "  pop rax\n");

    if (ty->kind == TY_BOOL) {
        fprintf(cout, "  cmp rdi, 0\n");
        fprintf(cout, "  setne dil\n");
        fprintf(cout, "  movzb rdi, dil\n");
    }

    int sz = size_of(ty, NULL);
    if (sz == 1) {
        fprintf(cout, "  mov [rax], dil\n");
    } else if (sz == 2) {
        fprintf(cout, "  mov [rax], di\n");
    } else if (sz == 4) {
        fprintf(cout, "  mov [rax], edi\n");
    } else {
        assert(sz == 8);
        fprintf(cout, "  mov [rax], rdi\n");
    }

    fprintf(cout, "  push rdi\n");
}

void truncate(Type *ty) {
    fprintf(cout, "  pop rax\n");

    if (ty->kind == TY_BOOL) {
        fprintf(cout, "  cmp rax, 0\n");
        fprintf(cout, "  setne al\n");
    }

    int sz = size_of(ty, NULL);
    if (sz == 1) {
        fprintf(cout, "  movsx rax, al\n");
    } else if (sz == 2) {
        fprintf(cout, "  movsx rax, ax\n");
    } else if (sz == 4) {
        fprintf(cout, "  movsxd rax, eax\n");
    }
    fprintf(cout, "  push rax\n");
}

void inc(Node *node) {
    int sz = node->ty->base ? size_of(node->ty->base, node->tok) : 1;
    fprintf(cout, "  pop rax\n");
    fprintf(cout, "  add rax, %d\n", sz);
    fprintf(cout, "  push rax\n");
}

void dec(Node *node) {
    int sz = node->ty->base ? size_of(node->ty->base, node->tok) : 1;
    fprintf(cout, "  pop rax\n");
    fprintf(cout, "  sub rax, %d\n", sz);
    fprintf(cout, "  push rax\n");
}

void gen(Node *node) {
    switch (node->kind) {
        case ND_NULL:
            return;
        case ND_NUM:
            if (node->val == (int)node->val) {
                fprintf(cout, "  push %ld\n", node->val);
            } else {
                fprintf(cout, "  movabs rax, %ld\n", node->val);
                fprintf(cout, "  push rax\n");
            }
            return;
        case ND_EXPR_STMT:
            gen(node->lhs);
            fprintf(cout, "  add rsp, 8\n");
            return;
        case ND_VAR:
        case ND_MEMBER:
            gen_addr(node);
            if (node->ty->kind != TY_ARRAY)
                load(node->ty);
            return;
        case ND_ASSIGN:
            gen_lval(node->lhs);
            gen(node->rhs);
            store(node->ty);
            return;
        case ND_TERNARY: {
            int seq = labelseq++;
            gen(node->cond);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  je  .Lelse%d\n", seq);
            gen(node->then);
            fprintf(cout, "  jmp .Lend%d\n", seq);
            fprintf(cout, ".Lelse%d:\n", seq);
            gen(node->els);
            fprintf(cout, ".Lend%d:\n", seq);
            return;
        }
        case ND_PRE_INC:
            gen_lval(node->lhs);
            fprintf(cout, "  push [rsp]\n");
            load(node->ty);
            inc(node);
            store(node->ty);
            return;
        case ND_PRE_DEC:
            gen_lval(node->lhs);
            fprintf(cout, "  push [rsp]\n");
            load(node->ty);
            dec(node);
            store(node->ty);
            return;
        case ND_POST_INC:
            gen_lval(node->lhs);
            fprintf(cout, "  push [rsp]\n");
            load(node->ty);
            inc(node);
            store(node->ty);
            dec(node);
            return;
        case ND_POST_DEC:
            gen_lval(node->lhs);
            fprintf(cout, "  push [rsp]\n");
            load(node->ty);
            dec(node);
            store(node->ty);
            inc(node);
            return;
        case ND_A_ADD:
        case ND_A_SUB:
        case ND_A_MUL:
        case ND_A_DIV:
        case ND_A_SHL:
        case ND_A_SHR: {
            gen_lval(node->lhs);
            fprintf(cout, "  push [rsp]\n");
            load(node->lhs->ty);
            gen(node->rhs);
            fprintf(cout, "  pop rdi\n");
            fprintf(cout, "  pop rax\n");

            switch (node->kind) {
                case ND_A_ADD:
                    if (node->ty->base)
                        fprintf(cout, "  imul rdi, %d\n", size_of(node->ty->base, node->tok));
                    fprintf(cout, "  add rax, rdi\n");
                    break;
                case ND_A_SUB:
                    if (node->ty->base)
                        fprintf(cout, "  imul rdi, %d\n", size_of(node->ty->base, node->tok));
                    fprintf(cout, "  sub rax, rdi\n");
                    break;
                case ND_A_MUL:
                    fprintf(cout, "  imul rax, rdi\n");
                    break;
                case ND_A_DIV:
                    fprintf(cout, "  cqo\n");
                    fprintf(cout, "  idiv rdi\n");
                    break;
                case ND_A_SHL:
                    fprintf(cout, "  mov cl, dil\n");
                    fprintf(cout, "  shl rax, cl\n");
                    break;
                case ND_A_SHR:
                    fprintf(cout, "  mov cl, dil\n");
                    fprintf(cout, "  sar rax, cl\n");
                    break;
            }

            fprintf(cout, "  push rax\n");
            store(node->ty);
            return;
        }
        case ND_COMMA:
            gen(node->lhs);
            gen(node->rhs);
            return;
        case ND_ADDR:
            gen_addr(node->lhs);
            return;
        case ND_DEREF:
            gen(node->lhs);
            if (node->ty->kind != TY_ARRAY)
                load(node->ty);
            return;
        case ND_NOT:
            gen(node->lhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  sete al\n");
            fprintf(cout, "  movzb rax, al\n");
            fprintf(cout, "  push rax\n");
            return;
        case ND_BITNOT:
            gen(node->lhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  not rax\n");
            fprintf(cout, "  push rax\n");
            return;
        case ND_LOGAND: {
            int seq = labelseq++;
            gen(node->lhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  je  .Lfalse%d\n", seq);
            gen(node->rhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  je  .Lfalse%d\n", seq);
            fprintf(cout, "  push 1\n");
            fprintf(cout, "  jmp .Lend%d\n", seq);
            fprintf(cout, ".Lfalse%d:\n", seq);
            fprintf(cout, "  push 0\n");
            fprintf(cout, ".Lend%d:\n", seq);
            return;
        }
        case ND_LOGOR: {
            int seq = labelseq++;
            gen(node->lhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  jne .Ltrue%d\n", seq);
            gen(node->rhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  jne .Ltrue%d\n", seq);
            fprintf(cout, "  push 0\n");
            fprintf(cout, "  jmp .Lend%d\n", seq);
            fprintf(cout, ".Ltrue%d:\n", seq);
            fprintf(cout, "  push 1\n");
            fprintf(cout, ".Lend%d:\n", seq);
            return;
        }
        case ND_IF: {
            int seq = labelseq++;
            if (node->els) {
                gen(node->cond);
                fprintf(cout, "  pop rax\n");
                fprintf(cout, "  cmp rax, 0\n");
                fprintf(cout, "  je  .Lelse%d\n", seq);
                gen(node->then);
                fprintf(cout, "  jmp .Lend%d\n", seq);
                fprintf(cout, ".Lelse%d:\n", seq);
                gen(node->els);
                fprintf(cout, ".Lend%d:\n", seq);
            } else {
                gen(node->cond);
                fprintf(cout, "  pop rax\n");
                fprintf(cout, "  cmp rax, 0\n");
                fprintf(cout, "  je  .Lend%d\n", seq);
                gen(node->then);
                fprintf(cout, ".Lend%d:\n", seq);
            }
            return;
        }
        case ND_WHILE: {
            int seq = labelseq++;
            int brk = brkseq;
            int cont = contseq;
            brkseq = contseq = seq;

            fprintf(cout, ".L.continue.%d:\n", seq);
            gen(node->cond);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  cmp rax, 0\n");
            fprintf(cout, "  je  .L.break.%d\n", seq);
            gen(node->then);
            fprintf(cout, "  jmp .L.continue.%d\n", seq);
            fprintf(cout, ".L.break.%d:\n", seq);

            brkseq = brk;
            contseq = cont;
            return;
        }
        case ND_FOR: {
            int seq = labelseq++;
            int brk = brkseq;
            int cont = contseq;
            brkseq = contseq = seq;

            if (node->init)
                gen(node->init);
            fprintf(cout, ".Lbegin%d:\n", seq);
            if (node->cond) {
                gen(node->cond);
                fprintf(cout, "  pop rax\n");
                fprintf(cout, "  cmp rax, 0\n");
                fprintf(cout, "  je  .L.break.%d\n", seq);
            }
            gen(node->then);
            fprintf(cout, ".L.continue.%d:\n", seq);
            if (node->inc)
                gen(node->inc);
            fprintf(cout, "  jmp .Lbegin%d\n", seq);
            fprintf(cout, ".L.break.%d:\n", seq);

            brkseq = brk;
            contseq = cont;
            return;
        }
        case ND_SWITCH: {
            int seq = labelseq++;
            int brk = brkseq;
            brkseq = seq;
            node->case_label = seq;

            gen(node->cond);
            fprintf(cout, "  pop rax\n");

            for (Node *n = node->case_next; n; n = n->case_next) {
                n->case_label = labelseq++;
                n->case_end_label = seq;
                fprintf(cout, "  cmp rax, %ld\n", n->val);
                fprintf(cout, "  je .L.case.%d\n", n->case_label);
            }

            if (node->default_case) {
                int i = labelseq++;
                node->default_case->case_end_label = seq;
                node->default_case->case_label = i;
                fprintf(cout, "  jmp .L.case.%d\n", i);
            }

            fprintf(cout, "  jmp .L.break.%d\n", seq);
            gen(node->then);
            fprintf(cout, ".L.break.%d:\n", seq);

            brkseq = brk;
            return;
        }
        case ND_CASE:
            fprintf(cout, ".L.case.%d:\n", node->case_label);
            gen(node->lhs);
            fprintf(cout, "  jmp .L.break.%d\n", node->case_end_label);
            return;
        case ND_BLOCK:
        case ND_STMT_EXPR:
            for (Node *n = node->body; n; n = n->next)
                gen(n);
            return;
        case ND_BREAK:
            if (brkseq == 0)
                error_tok(node->tok, "stray break");
            fprintf(cout, "  jmp .L.break.%d\n", brkseq);
            return;
        case ND_CONTINUE:
            if (contseq == 0)
                error_tok(node->tok, "stray continue");
            fprintf(cout, "  jmp .L.continue.%d\n", contseq);
            return;
        case ND_GOTO:
            fprintf(cout, "  jmp .L.label.%s.%s\n", funcname, node->label_name);
            return;
        case ND_LABEL:
            fprintf(cout, ".L.label.%s.%s:\n", funcname, node->label_name);
            gen(node->lhs);
            return;
        case ND_FUNCALL: {
            int nargs = 0;
            for (Node *arg = node->args; arg; arg = arg->next) {
                gen(arg);
                nargs++;
            }

            for (int i = nargs - 1; i >= 0; i--)
                fprintf(cout, "  pop %s\n", argreg8[i]);

            int seq = labelseq++;
            fprintf(cout, "  mov rax, rsp\n");
            fprintf(cout, "  and rax, 15\n");
            fprintf(cout, "  jnz .Lcall%d\n", seq);
            fprintf(cout, "  mov rax, 0\n");
            fprintf(cout, "  call %s\n", node->funcname);
            fprintf(cout, "  jmp .Lend%d\n", seq);
            fprintf(cout, ".Lcall%d:\n", seq);
            fprintf(cout, "  sub rsp, 8\n");
            fprintf(cout, "  mov rax, 0\n");
            fprintf(cout, "  call %s\n", node->funcname);
            fprintf(cout, "  add rsp, 8\n");
            fprintf(cout, ".Lend%d:\n", seq);
            fprintf(cout, "  push rax\n");

            if (node->ty->kind != TY_VOID)
                truncate(node->ty);
            return;
        }
        case ND_RETURN:
            gen(node->lhs);
            fprintf(cout, "  pop rax\n");
            fprintf(cout, "  jmp .Lreturn.%s\n", funcname);
            return;
        case ND_CAST:
            gen(node->lhs);
            truncate(node->ty);
            return;
    }

    gen(node->lhs);
    gen(node->rhs);

    fprintf(cout, "  pop rdi\n");
    fprintf(cout, "  pop rax\n");

    switch (node->kind) {
        case ND_ADD:
            if (node->ty->base)
                fprintf(cout, "  imul rdi, %d\n", size_of(node->ty->base, node->tok));
            fprintf(cout, "  add rax, rdi\n");
            break;
        case ND_SUB:
            if (node->ty->base)
                fprintf(cout, "  imul rdi, %d\n", size_of(node->ty->base, node->tok));
            fprintf(cout, "  sub rax, rdi\n");
            break;
        case ND_MUL:
            fprintf(cout, "  imul rax, rdi\n");
            break;
        case ND_DIV:
            fprintf(cout, "  cqo\n");
            fprintf(cout, "  idiv rdi\n");
            break;
        case ND_BITAND:
            fprintf(cout, "  and rax, rdi\n");
            break;
        case ND_BITOR:
            fprintf(cout, "  or rax, rdi\n");
            break;
        case ND_BITXOR:
            fprintf(cout, "  xor rax, rdi\n");
            break;
        case ND_SHL:
            fprintf(cout, "  mov cl, dil\n");
            fprintf(cout, "  shl rax, cl\n");
            break;
        case ND_SHR:
            fprintf(cout, "  mov cl, dil\n");
            fprintf(cout, "  sar rax, cl\n");
            break;
        case ND_EQ:
            fprintf(cout, "  cmp rax, rdi\n");
            fprintf(cout, "  sete al\n");
            fprintf(cout, "  movzb rax, al\n");
            break;
        case ND_NE:
            fprintf(cout, "  cmp rax, rdi\n");
            fprintf(cout, "  setne al\n");
            fprintf(cout, "  movzb rax, al\n");
            break;
        case ND_LT:
            fprintf(cout, "  cmp rax, rdi\n");
            fprintf(cout, "  setl al\n");
            fprintf(cout, "  movzb rax, al\n");
            break;
        case ND_LE:
            fprintf(cout, "  cmp rax, rdi\n");
            fprintf(cout, "  setle al\n");
            fprintf(cout, "  movzb rax, al\n");
            break;
    }

    fprintf(cout, "  push rax\n");
}

void emit_data(Program *prog) {
    fprintf(cout, ".data\n");

    for (VarList *vl = prog->globals; vl; vl = vl->next) {
        Var *var = vl->var;
        fprintf(cout, "%s:\n", var->name);

        if (!var->initializer) {
            fprintf(cout, "  .zero %d\n", size_of(var->ty, var->tok));
            continue;
        }

        for (Initializer *init = var->initializer; init; init = init->next) {
            if (init->label) {
                fprintf(cout, "  .quad %s%+ld\n", init->label, init->addend);
                continue;
            }

            if (init->sz == 1)
                fprintf(cout, "  .byte %ld\n", init->val);
            else
                fprintf(cout, "  .%dbyte %ld\n", init->sz, init->val);
        }
    }
}

void load_arg(Var *var, int idx) {
    int sz = size_of(var->ty, var->tok);
    if (sz == 1) {
        fprintf(cout, "  mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
    } else if (sz == 2) {
        fprintf(cout, "  mov [rbp-%d], %s\n", var->offset, argreg2[idx]);
    } else if (sz == 4) {
        fprintf(cout, "  mov [rbp-%d], %s\n", var->offset, argreg4[idx]);
    } else {
        assert(sz == 8);
        fprintf(cout, "  mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
    }
}

void emit_text(Program *prog) {
    fprintf(cout, ".text\n");

    for (Function *fn = prog->fns; fn; fn = fn->next) {
        if (fn->pub)
            fprintf(cout, ".global %s\n", fn->name);
        fprintf(cout, "%s:\n", fn->name);
        funcname = fn->name;

        fprintf(cout, "  push rbp\n");
        fprintf(cout, "  mov rbp, rsp\n");
        fprintf(cout, "  sub rsp, %d\n", fn->stack_size);

        int i = 0;
        for (VarList *vl = fn->params; vl; vl = vl->next)
            load_arg(vl->var, i++);

        for (Node *node = fn->node; node; node = node->next)
            gen(node);

        fprintf(cout, ".Lreturn.%s:\n", funcname);
        fprintf(cout, "  mov rsp, rbp\n");
        fprintf(cout, "  pop rbp\n");
        fprintf(cout, "  ret\n");
    }
}

void codegen(Program *prog) {
    fprintf(cout, ".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}