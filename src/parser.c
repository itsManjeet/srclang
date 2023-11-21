#include "srclang.h"

typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    int depth;
    Var *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
};

typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    int depth;
    Type *ty;
};

typedef struct {
    VarScope *var_scope;
    TagScope *tag_scope;
} Scope;

VarList *locals;
VarList *globals;

VarScope *var_scope;
TagScope *tag_scope;
int scope_depth;

Node *current_switch;

Scope *enter_scope() {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    ++scope_depth;
    return sc;
}

void leave_scope(Scope *sc) {
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
    --scope_depth;
}

VarScope *find_var(Token *tok) {
    for (VarScope *sc = var_scope; sc; sc = sc->next) {
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len))
            return sc;
    }
    return NULL;
}

TagScope *find_tag(Token *tok) {
    for (TagScope *sc = tag_scope; sc; sc = sc->next)
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len))
            return sc;
    return NULL;
}

Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

Node *new_num(long val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

Node *new_var(Var *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

VarScope *push_scope(char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    sc->depth = scope_depth;
    var_scope = sc;
    return sc;
}

Var *push_var(char *name, Type *ty, bool is_local, Token *tok) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;
    var->tok = tok;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    if (is_local) {
        vl->next = locals;
        locals = vl;
    } else if (ty->kind != TY_FUNC) {
        vl->next = globals;
        globals = vl;
    }
    return var;
}

Type *find_typedef(Token *tok) {
    if (tok->kind == TK_IDENT) {
        VarScope *sc = find_var(token);
        if (sc)
            return sc->type_def;
    }
    return NULL;
}

char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

Function *function(bool pub);
Type *type_specifier();
Type *declarator(Type *ty);
Type *abstract_declarator(Type *ty);
Type *type_suffix(Type *ty);
void type_define();
Type *type_name();
Type *struct_decl();
Type *enum_specifier();
Member *struct_member();
void global_var(bool pub);
Node *declaration();
bool is_typename();
Node *stmt();
Node *expr();
long eval(Node *node);
long eval2(Node *node, Var **var);
long const_expr();
Node *assign();
Node *conditional();
Node *logor();
Node *logand();
Node *bitand();
Node * bitor ();
Node *bitxor();
Node *equality();
Node *relational();
Node *shift();
Node *add();
Node *mul();
Node *cast();
Node *unary();
Node *postfix();
Node *primary();

static char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        error("cannot open %s: %s", path, strerror(errno));

    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(filesize + 2);
    int size = fread(buf, 1, filesize, fp);
    if (size != filesize)
        error("file too large %lld != %lld", filesize, size);

    buf[filesize] = '\n';
    buf[filesize + 1] = '\0';
    return buf;
}

static Program *use() {
    char path[1024];
    char *id = expect_ident();
    strcpy(path, id);
    while (consume(".")) {
        strcat(path, "/");
        strcat(path, expect_ident());
    }
    expect(";");
    strcat(path, ".src");

    Token *tok_backup = token;
    char *user_input_backup = user_input;
    char *filename_backup = filename;

    filename = path;
    user_input = read_file(path);
    token = tokenize();

    Program *prog = program();
    token = tok_backup;
    free(user_input);
    user_input = user_input_backup;
    filename = filename_backup;

    return program();
}

Program *program() {
    Function head;
    head.next = NULL;
    Function *cur = &head;
    globals = NULL;

    bool pub = false;

    while (!at_eof()) {
        pub = consume("pub") != NULL;

        if (consume("fun")) {
            Function *fun = function(pub);
            if (!fun) continue;

            cur->next = fun;
            cur = cur->next;
        } else if (consume("let")) {
            global_var(pub);
        } else if (consume("type")) {
            type_define();
        } else if (consume("use")) {
            VarList *backup = globals;
            Program *prog = use();

            for (Function *fun = prog->fns; fun; fun = fun->next) {
                if (fun->pub) {
                    cur->next = fun;
                    cur = cur->next;
                }
            }
            globals = backup;
        } else {
            error("invalid global declaration");
        }
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

Type *type_specifier() {
    if (!is_typename(token))
        error_tok(token, "typename expected");

    Type *ty = NULL;

    enum {
        VOID = 1 << 1,
        BOOL = 1 << 3,
        CHAR = 1 << 5,
        SHORT = 1 << 7,
        INT = 1 << 9,
        LONG = 1 << 11,
    };

    int base_type = 0;
    Type *user_type = NULL;

    bool is_static = false;

    Token *tok = token;
    if (consume("static")) {
        is_static = true;
    }

    if (consume("none")) {
        base_type = VOID;
    } else if (consume("bool")) {
        base_type = BOOL;
    } else if (consume("i8")) {
        base_type = CHAR;
    } else if (consume("i16")) {
        base_type = SHORT;
    } else if (consume("i32")) {
        base_type = INT;
    } else if (consume("i64")) {
        base_type = LONG;
    } else if (peek("struct")) {
        user_type = struct_decl();
    } else if (peek("enum")) {
        user_type = enum_specifier();
    } else {
        Type *ty = find_typedef(token);
        if (!ty) error("unknown type");

        token = token->next;
        user_type = ty;
    }

    switch (base_type) {
        case VOID:
            ty = void_type();
            break;
        case BOOL:
            ty = bool_type();
            break;
        case CHAR:
            ty = char_type();
            break;
        case SHORT:
            ty = short_type();
            break;
        case INT:
            ty = int_type();
            break;
        case LONG:
            ty = long_type();
            break;
        case 0:
            ty = user_type ? user_type : int_type();
            break;
        default:
            error_tok(tok, "invalid type");
    }

    ty->is_static = is_static;
    return ty;
}

Type *declarator(Type *ty) {
    while (consume("*"))
        ty = pointer_to(ty);

    if (consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = declarator(placeholder);
        expect(")");
        *placeholder = *type_suffix(ty);
        return new_ty;
    }
    return type_suffix(ty);
}

Type *abstract_declarator(Type *ty) {
    while (consume("*"))
        ty = pointer_to(ty);

    if (consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = abstract_declarator(placeholder);
        expect(")");
        *placeholder = *type_suffix(ty);
        return new_ty;
    }
    return type_suffix(ty);
}

Type *type_suffix(Type *ty) {
    if (!consume("["))
        return ty;

    int sz = 0;
    bool is_incomplete = true;
    if (!consume("]")) {
        sz = const_expr();
        is_incomplete = false;
        expect("]");
    }

    ty = type_suffix(ty);
    ty = array_of(ty, sz);
    ty->is_incomplete = is_incomplete;
    return ty;
}

Type *type_name() {
    Type *ty = type_specifier();
    ty = abstract_declarator(ty);
    return type_suffix(ty);
}

void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->depth = scope_depth;
    sc->ty = ty;
    tag_scope = sc;
}

Type *struct_decl() {
    expect("struct");
    Token *tag = consume_ident();
    if (tag && !peek("{")) {
        TagScope *sc = find_tag(tag);

        if (!sc) {
            Type *ty = struct_type();
            push_tag_scope(tag, ty);
            return ty;
        }

        if (sc->ty->kind != TY_STRUCT)
            error_tok(tag, "not a struct tag");
        return sc->ty;
    }

    if (!consume("{"))
        return struct_type();

    TagScope *sc = find_tag(tag);
    Type *ty;

    if (sc && sc->depth == scope_depth) {
        if (sc->ty->kind != TY_STRUCT)
            error_tok(tag, "not a struct tag");
        ty = sc->ty;
    } else {
        ty = struct_type();
        if (tag)
            push_tag_scope(tag, ty);
    }

    Member head;
    head.next = NULL;
    Member *cur = &head;
    while (!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;

        if (consume("}")) break;
        expect(",");
    }

    ty->members = head.next;

    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += size_of(mem->ty, mem->tok);

        if (ty->align < mem->ty->align)
            ty->align = mem->ty->align;
    }

    ty->is_incomplete = false;
    return ty;
}

Type *enum_specifier() {
    expect("enum");
    Type *ty = enum_type();

    Token *tag = consume_ident();
    if (tag && !peek("{")) {
        TagScope *sc = find_tag(tag);
        if (!sc)
            error_tok(tag, "unknown enum type");
        if (sc->ty->kind != TY_ENUM)
            error_tok(tag, "not an enum tag");
        return sc->ty;
    }

    expect("{");

    int cnt = 0;
    for (;;) {
        char *name = expect_ident();
        if (consume("="))
            cnt = const_expr();

        VarScope *sc = push_scope(name);
        sc->enum_ty = ty;
        sc->enum_val = cnt++;

        if (consume(",")) {
            if (consume("}"))
                break;
            continue;
        }
        expect("}");
        break;
    }

    if (tag)
        push_tag_scope(tag, ty);
    return ty;
}

Member *struct_member() {
    Token *pos = token;
    char *id = expect_ident();

    expect(":");

    Type *type = type_specifier();
    type = declarator(type);
    type = type_suffix(type);

    Member *mem = calloc(1, sizeof(Member));
    mem->name = id;
    mem->ty = type;
    mem->tok = pos;
    return mem;
}

VarList *read_func_param() {
    Token *pos = token;
    char *id = expect_ident();

    expect(":");

    Type *type = type_specifier();
    type = declarator(type);
    type = type_suffix(type);

    if (type->kind == TY_ARRAY)
        type = pointer_to(type->base);

    Var *var = push_var(id, type, true, pos);
    push_scope(id)->var = var;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    return vl;
}

VarList *read_func_params() {
    if (consume(")"))
        return NULL;

    Token *tok = token;
    if (consume("void") && consume(")"))
        return NULL;
    if (consume("...")) {
        expect(")");
        return NULL;
    }
    token = tok;

    VarList *head = read_func_param();
    VarList *cur = head;

    while (!consume(")")) {
        expect(",");
        if (consume("...")) {
            expect(")");
            break;
        }
        cur->next = read_func_param();
        cur = cur->next;
    }

    return head;
}

Function *function(bool pub) {
    locals = NULL;
    Function *fun = NULL;
    fun = calloc(1, sizeof(Function));

    Token *pos = token;
    fun->name = expect_ident();
    fun->pub = (strcmp(fun->name, "main") == 0) ? true : pub;

    expect("(");
    fun->params = read_func_params();

    fun->ret_type = type_specifier();
    fun->ret_type = declarator(fun->ret_type);

    if (consume(";"))
        return NULL;

    Var *var = push_var(fun->name, func_type(fun->ret_type), false, pos);
    push_scope(fun->name)->var = var;

    Node head;
    head.next = NULL;
    Node *cur = &head;
    expect("{");
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }

    fun->node = head.next;
    fun->locals = locals;
    return fun;
}

bool peek_end() {
    Token *tok = token;
    bool ret = consume("}") || (consume(",") && consume("}"));
    token = tok;
    return ret;
}

bool consume_end() {
    Token *tok = token;
    if (consume("}") || (consume(",") && consume("}")))
        return true;
    token = tok;
    return false;
}

Initializer *new_init_val(Initializer *cur, int sz, int val) {
    Initializer *init = calloc(1, sizeof(Initializer));
    init->sz = sz;
    init->val = val;
    cur->next = init;
    return init;
}

Initializer *new_init_label(Initializer *cur, char *label, long addend) {
    Initializer *init = calloc(1, sizeof(Initializer));
    init->label = label;
    init->addend = addend;
    cur->next = init;
    return init;
}

Initializer *new_init_zero(Initializer *cur, int nbytes) {
    for (int i = 0; i < nbytes; i++)
        cur = new_init_val(cur, 1, 0);
    return cur;
}

Initializer *gvar_init_string(char *p, int len) {
    Initializer head;
    head.next = NULL;
    Initializer *cur = &head;
    for (int i = 0; i < len; i++)
        cur = new_init_val(cur, 1, p[i]);
    return head.next;
}

Initializer *emit_struct_padding(Initializer *cur, Type *parent, Member *mem) {
    int end = mem->offset + size_of(mem->ty, token);

    int padding;
    if (mem->next)
        padding = mem->next->offset - end;
    else
        padding = size_of(parent, token) - end;

    if (padding)
        cur = new_init_zero(cur, padding);
    return cur;
}

void skip_excess_elements2() {
    for (;;) {
        if (consume("{"))
            skip_excess_elements2();
        else
            assign();

        if (consume_end())
            return;
        expect(",");
    }
}

void skip_excess_elements() {
    expect(",");
    warn_tok(token, "excess elements in initializer");
    skip_excess_elements2();
}

Initializer *gvar_initializer(Initializer *cur, Type *ty) {
    Token *tok = token;

    if (ty->kind == TY_ARRAY && ty->base->kind == TY_CHAR &&
        token->kind == TK_STR) {
        token = token->next;

        if (ty->is_incomplete) {
            ty->array_size = tok->cont_len;
            ty->is_incomplete = false;
        }

        int len = (ty->array_size < tok->cont_len)
                      ? ty->array_size
                      : tok->cont_len;

        for (int i = 0; i < len; i++)
            cur = new_init_val(cur, 1, tok->contents[i]);
        return new_init_zero(cur, ty->array_size - len);
    }

    if (ty->kind == TY_ARRAY) {
        bool open = consume("{");
        int i = 0;
        int limit = ty->is_incomplete ? INT_MAX : ty->array_size;

        do {
            cur = gvar_initializer(cur, ty->base);
            i++;
        } while (i < limit && !peek_end() && consume(","));

        if (open && !consume_end())
            skip_excess_elements();

        cur = new_init_zero(cur, size_of(ty->base, tok) * (ty->array_size - i));

        if (ty->is_incomplete) {
            ty->array_size = i;
            ty->is_incomplete = false;
        }

        return cur;
    }

    if (ty->kind == TY_STRUCT) {
        bool open = consume("{");
        Member *mem = ty->members;

        do {
            cur = gvar_initializer(cur, mem->ty);
            cur = emit_struct_padding(cur, ty, mem);
            mem = mem->next;
        } while (mem && !peek_end() && consume(","));

        if (open && !consume_end())
            skip_excess_elements();

        if (mem) {
            int sz = size_of(ty, tok) - mem->offset;
            if (sz)
                cur = new_init_zero(cur, sz);
        }
        return cur;
    }

    bool open = consume("{");
    Node *expr = conditional();
    if (open)
        expect("}");

    Var *var = NULL;
    long addend = eval2(expr, &var);

    if (var)
        return new_init_label(cur, var->name, addend);
    return new_init_val(cur, size_of(ty, token), addend);
}

void global_var(bool pub) {
    Token *pos = token;
    char *id = expect_ident();
    expect(":");

    Type *type = type_specifier();
    type = declarator(type);
    type = type_suffix(type);

    Var *var = push_var(id, type, false, pos);
    push_scope(id)->var = var;

    if (consume("=")) {
        Initializer head;
        head.next = NULL;
        gvar_initializer(&head, type);
        var->initializer = head.next;
    }
    expect(";");
}

void type_define() {
    Token *pos = token;
    char *id = expect_ident();

    Type *type = type_specifier();
    type = declarator(type);
    type = type_suffix(type);

    push_scope(id)->type_def = type;
    expect(";");
}

typedef struct Designator Designator;
struct Designator {
    Designator *next;
    int idx;
    Member *mem;
};

Node *new_desg_node2(Var *var, Designator *desg) {
    Token *tok = var->tok;
    if (!desg)
        return new_var(var, tok);

    Node *node = new_desg_node2(var, desg->next);

    if (desg->mem) {
        node = new_unary(ND_MEMBER, node, desg->mem->tok);
        node->member_name = desg->mem->name;
        return node;
    }

    node = new_binary(ND_ADD, node, new_num(desg->idx, tok), tok);
    return new_unary(ND_DEREF, node, tok);
}

Node *new_desg_node(Var *var, Designator *desg, Node *rhs) {
    Node *lhs = new_desg_node2(var, desg);
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, rhs->tok);
    return new_unary(ND_EXPR_STMT, node, rhs->tok);
}

Node *lvar_init_zero(Node *cur, Var *var, Type *ty, Designator *desg) {
    if (ty->kind == TY_ARRAY) {
        for (int i = 0; i < ty->array_size; i++) {
            Designator desg2 = {desg, i++, NULL};
            cur = lvar_init_zero(cur, var, ty->base, &desg2);
        }
        return cur;
    }

    cur->next = new_desg_node(var, desg, new_num(0, token));
    return cur->next;
}

Node *lvar_initializer(Node *cur, Var *var, Type *ty, Designator *desg) {
    if (ty->kind == TY_ARRAY && ty->base->kind == TY_CHAR &&
        token->kind == TK_STR) {
        Token *tok = token;
        token = token->next;

        if (ty->is_incomplete) {
            ty->array_size = tok->cont_len;
            ty->is_incomplete = false;
        }

        int len = (ty->array_size < tok->cont_len)
                      ? ty->array_size
                      : tok->cont_len;
        int i;

        for (i = 0; i < len; i++) {
            Designator desg2 = {desg, i, NULL};
            Node *rhs = new_num(tok->contents[i], tok);
            cur->next = new_desg_node(var, &desg2, rhs);
            cur = cur->next;
        }

        for (; i < ty->array_size; i++) {
            Designator desg2 = {desg, i, NULL};
            cur = lvar_init_zero(cur, var, ty->base, &desg2);
        }
        return cur;
    }

    if (ty->kind == TY_ARRAY) {
        bool open = consume("{");
        int i = 0;
        int limit = ty->is_incomplete ? INT_MAX : ty->array_size;

        do {
            Designator desg2 = {desg, i++, NULL};
            cur = lvar_initializer(cur, var, ty->base, &desg2);
        } while (i < limit && !peek_end() && consume(","));

        if (open && !consume_end())
            skip_excess_elements();

        while (i < ty->array_size) {
            Designator desg2 = {desg, i++, NULL};
            cur = lvar_init_zero(cur, var, ty->base, &desg2);
        }

        if (ty->is_incomplete) {
            ty->array_size = i;
            ty->is_incomplete = false;
        }
        return cur;
    }

    if (ty->kind == TY_STRUCT) {
        bool open = consume("{");
        Member *mem = ty->members;

        do {
            Designator desg2 = {desg, 0, mem};
            cur = lvar_initializer(cur, var, mem->ty, &desg2);
            mem = mem->next;
        } while (mem && !peek_end() && consume(","));

        if (open && !consume_end())
            skip_excess_elements();

        for (; mem; mem = mem->next) {
            Designator desg2 = {desg, 0, mem};
            cur = lvar_init_zero(cur, var, mem->ty, &desg2);
        }
        return cur;
    }

    bool open = consume("{");
    cur->next = new_desg_node(var, desg, assign());
    if (open)
        expect("}");
    return cur->next;
}

Node *declaration() {
    Token *pos = token;
    char *id = expect_ident();
    expect(":");

    Type *type = type_specifier();
    type = declarator(type);
    type = type_suffix(type);

    // if (type->is_typedef) {
    //     expect(";");
    //     ty->is_typedef = false;
    //     push_scope(name)->type_def = ty;
    //     return new_node(ND_NULL, tok);
    // }

    if (type->kind == TY_VOID)
        error_tok(pos, "variable declared void");

    Var *var;
    if (type->is_static)
        var = push_var(new_label(), type, false, pos);
    else
        var = push_var(id, type, true, pos);
    push_scope(id)->var = var;

    if (consume(";"))
        return new_node(ND_NULL, pos);

    expect("=");

    Node head;
    head.next = NULL;
    lvar_initializer(&head, var, var->ty, NULL);
    expect(";");

    Node *node = new_node(ND_BLOCK, pos);
    node->body = head.next;
    return node;
}

Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

bool is_typename() {
    return peek("none") || peek("bool") || peek("i8") || peek("i16") ||
           peek("i32") || peek("i64") || peek("enum") || peek("struct") ||
           peek("static") || find_typedef(token);
}

Node *stmt() {
    Token *tok;
    if (tok = consume("return")) {
        Node *node = new_unary(ND_RETURN, expr(), tok);
        expect(";");
        return node;
    }

    if (tok = consume("if")) {
        Node *node = new_node(ND_IF, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else"))
            node->els = stmt();
        return node;
    }

    if (tok = consume("switch")) {
        Node *node = new_node(ND_SWITCH, tok);
        expect("(");
        node->cond = expr();
        expect(")");

        Node *sw = current_switch;
        current_switch = node;
        node->then = stmt();
        current_switch = sw;
        return node;
    }

    if (tok = consume("case")) {
        if (!current_switch)
            error_tok(tok, "stray case");
        int val = const_expr();
        expect(":");

        Node *node = new_unary(ND_CASE, stmt(), tok);
        node->val = val;
        node->case_next = current_switch->case_next;
        current_switch->case_next = node;
        return node;
    }

    if (tok = consume("default")) {
        if (!current_switch)
            error_tok(tok, "stray default");
        expect(":");

        Node *node = new_unary(ND_CASE, stmt(), tok);
        current_switch->default_case = node;
        return node;
    }

    if (tok = consume("while")) {
        Node *node = new_node(ND_WHILE, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    if (tok = consume("for")) {
        Node *node = new_node(ND_FOR, tok);
        expect("(");
        Scope *sc = enter_scope();

        if (!consume(";")) {
            if (consume("let")) {
                node->init = declaration();
            } else {
                node->init = read_expr_stmt();
                expect(";");
            }
        }
        if (!consume(";")) {
            node->cond = expr();
            expect(";");
        }
        if (!consume(")")) {
            node->inc = read_expr_stmt();
            expect(")");
        }
        node->then = stmt();

        leave_scope(sc);
        return node;
    }

    if (tok = consume("{")) {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        Scope *sc = enter_scope();
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        leave_scope(sc);

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    if (tok = consume("break")) {
        expect(";");
        return new_node(ND_BREAK, tok);
    }

    if (tok = consume("continue")) {
        expect(";");
        return new_node(ND_CONTINUE, tok);
    }

    if (tok = consume("goto")) {
        Node *node = new_node(ND_GOTO, tok);
        node->label_name = expect_ident();
        expect(";");
        return node;
    }

    if (tok = consume_ident()) {
        if (consume(":")) {
            Node *node = new_unary(ND_LABEL, stmt(), tok);
            node->label_name = strndup(tok->str, tok->len);
            return node;
        }
        token = tok;
    }

    if (tok = consume("let")) {
        return declaration();
    }

    Node *node = read_expr_stmt();
    expect(";");
    return node;
}

Node *expr() {
    Node *node = assign();
    Token *tok;
    while (tok = consume(",")) {
        node = new_unary(ND_EXPR_STMT, node, node->tok);
        node = new_binary(ND_COMMA, node, assign(), tok);
    }
    return node;
}

long eval(Node *node) {
    return eval2(node, NULL);
}

long eval2(Node *node, Var **var) {
    switch (node->kind) {
        case ND_ADD: {
            long lhs = eval2(node->lhs, var);
            return lhs + eval2(node->rhs, var);
        }
        case ND_SUB: {
            long lhs = eval2(node->lhs, var);
            return lhs - eval(node->rhs);
        }
        case ND_MUL:
            return eval(node->lhs) * eval(node->rhs);
        case ND_DIV:
            return eval(node->lhs) / eval(node->rhs);
        case ND_BITAND:
            return eval(node->lhs) & eval(node->rhs);
        case ND_BITOR:
            return eval(node->lhs) | eval(node->rhs);
        case ND_BITXOR:
            return eval(node->lhs) | eval(node->rhs);
        case ND_SHL:
            return eval(node->lhs) << eval(node->rhs);
        case ND_SHR:
            return eval(node->lhs) >> eval(node->rhs);
        case ND_EQ:
            return eval(node->lhs) == eval(node->rhs);
        case ND_NE:
            return eval(node->lhs) != eval(node->rhs);
        case ND_LT:
            return eval(node->lhs) < eval(node->rhs);
        case ND_LE:
            return eval(node->lhs) <= eval(node->rhs);
        case ND_TERNARY:
            return eval(node->cond) ? eval(node->then) : eval(node->els);
        case ND_COMMA:
            return eval(node->rhs);
        case ND_NOT:
            return !eval(node->lhs);
        case ND_BITNOT:
            return ~eval(node->lhs);
        case ND_LOGAND:
            return eval(node->lhs) && eval(node->rhs);
        case ND_LOGOR:
            return eval(node->lhs) || eval(node->rhs);
        case ND_NUM:
            return node->val;
        case ND_ADDR:
            if (!var || *var || node->lhs->kind != ND_VAR)
                error_tok(node->tok, "invalid initializer");
            *var = node->lhs->var;
            return 0;
        case ND_VAR:
            if (!var || *var || node->var->ty->kind != TY_ARRAY)
                error_tok(node->tok, "invalid initializer");
            *var = node->var;
            return 0;
    }

    error_tok(node->tok, "not a constant expression");
}

long const_expr() {
    return eval(conditional());
}

Node *assign() {
    Node *node = conditional();
    Token *tok;
    if (tok = consume("="))
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    if (tok = consume("+="))
        node = new_binary(ND_A_ADD, node, assign(), tok);
    if (tok = consume("-="))
        node = new_binary(ND_A_SUB, node, assign(), tok);
    if (tok = consume("*="))
        node = new_binary(ND_A_MUL, node, assign(), tok);
    if (tok = consume("/="))
        node = new_binary(ND_A_DIV, node, assign(), tok);
    if (tok = consume("<<="))
        node = new_binary(ND_A_SHL, node, assign(), tok);
    if (tok = consume(">>="))
        node = new_binary(ND_A_SHR, node, assign(), tok);
    return node;
}

Node *conditional() {
    Node *node = logor();
    Token *tok = consume("?");
    if (!tok)
        return node;

    Node *ternary = new_node(ND_TERNARY, tok);
    ternary->cond = node;
    ternary->then = expr();
    expect(":");
    ternary->els = conditional();
    return ternary;
}

Node *logor() {
    Node *node = logand();
    Token *tok;
    while (tok = consume("||"))
        node = new_binary(ND_LOGOR, node, logand(), tok);
    return node;
}

Node *logand() {
    Node *node = bitor ();
    Token *tok;
    while (tok = consume("&&"))
        node = new_binary(ND_LOGAND, node, bitor (), tok);
    return node;
}

Node * bitor () {
    Node *node = bitxor();
    Token *tok;
    while (tok = consume("|"))
        node = new_binary(ND_BITOR, node, bitxor(), tok);
    return node;
}

Node *bitxor() {
    Node *node = bitand();
    Token *tok;
    while (tok = consume("^"))
        node = new_binary(ND_BITXOR, node, bitxor(), tok);
    return node;
}

Node *bitand() {
    Node *node = equality();
    Token *tok;
    while (tok = consume("&"))
        node = new_binary(ND_BITAND, node, equality(), tok);
    return node;
}

Node *equality() {
    Node *node = relational();
    Token *tok;

    for (;;) {
        if (tok = consume("=="))
            node = new_binary(ND_EQ, node, relational(), tok);
        else if (tok = consume("!="))
            node = new_binary(ND_NE, node, relational(), tok);
        else
            return node;
    }
}

Node *relational() {
    Node *node = shift();
    Token *tok;

    for (;;) {
        if (tok = consume("<"))
            node = new_binary(ND_LT, node, shift(), tok);
        else if (tok = consume("<="))
            node = new_binary(ND_LE, node, shift(), tok);
        else if (tok = consume(">"))
            node = new_binary(ND_LT, shift(), node, tok);
        else if (tok = consume(">="))
            node = new_binary(ND_LE, shift(), node, tok);
        else
            return node;
    }
}

Node *shift() {
    Node *node = add();
    Token *tok;

    for (;;) {
        if (tok = consume("<<"))
            node = new_binary(ND_SHL, node, add(), tok);
        else if (tok = consume(">>"))
            node = new_binary(ND_SHR, node, add(), tok);
        else
            return node;
    }
}

Node *add() {
    Node *node = mul();
    Token *tok;

    for (;;) {
        if (tok = consume("+"))
            node = new_binary(ND_ADD, node, mul(), tok);
        else if (tok = consume("-"))
            node = new_binary(ND_SUB, node, mul(), tok);
        else
            return node;
    }
}

Node *mul() {
    Node *node = cast();
    Token *tok;

    for (;;) {
        if (tok = consume("*"))
            node = new_binary(ND_MUL, node, cast(), tok);
        else if (tok = consume("/"))
            node = new_binary(ND_DIV, node, cast(), tok);
        else
            return node;
    }
}

Node *cast() {
    Token *tok = token;

    if (consume("(")) {
        if (is_typename()) {
            Type *ty = type_name();
            expect(")");
            Node *node = new_unary(ND_CAST, cast(), tok);
            node->ty = ty;
            return node;
        }
        token = tok;
    }

    return unary();
}

Node *unary() {
    Token *tok;
    if (consume("+"))
        return cast();
    if (tok = consume("-"))
        return new_binary(ND_SUB, new_num(0, tok), cast(), tok);
    if (tok = consume("&"))
        return new_unary(ND_ADDR, cast(), tok);
    if (tok = consume("*"))
        return new_unary(ND_DEREF, cast(), tok);
    if (tok = consume("!"))
        return new_unary(ND_NOT, cast(), tok);
    if (tok = consume("~"))
        return new_unary(ND_BITNOT, cast(), tok);
    if (tok = consume("++"))
        return new_unary(ND_PRE_INC, unary(), tok);
    if (tok = consume("--"))
        return new_unary(ND_PRE_DEC, unary(), tok);
    return postfix();
}

Node *postfix() {
    Node *node = primary();
    Token *tok;

    for (;;) {
        if (tok = consume("[")) {
            Node *exp = new_binary(ND_ADD, node, expr(), tok);
            expect("]");
            node = new_unary(ND_DEREF, exp, tok);
            continue;
        }

        if (tok = consume(".")) {
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        if (tok = consume("->")) {
            node = new_unary(ND_DEREF, node, tok);
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        if (tok = consume("++")) {
            node = new_unary(ND_POST_INC, node, tok);
            continue;
        }

        if (tok = consume("--")) {
            node = new_unary(ND_POST_DEC, node, tok);
            continue;
        }

        return node;
    }
}

Node *stmt_expr(Token *tok) {
    Scope *sc = enter_scope();
    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");
    leave_scope(sc);

    if (cur->kind != ND_EXPR_STMT)
        error_tok(cur->tok, "stmt expr returning void is not supported");
    *cur = *cur->lhs;
    return node;
}

Node *func_args() {
    if (consume(")"))
        return NULL;

    Node *head = assign();
    Node *cur = head;
    while (consume(",")) {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return head;
}

Node *primary() {
    Token *tok;

    if (tok = consume("(")) {
        if (consume("{"))
            return stmt_expr(tok);

        Node *node = expr();
        expect(")");
        return node;
    }

    if (tok = consume("sizeof")) {
        if (consume("(")) {
            if (is_typename()) {
                Type *ty = type_name();
                expect(")");
                return new_num(size_of(ty, tok), tok);
            }
            token = tok->next;
        }
        return new_unary(ND_SIZEOF, unary(), tok);
    }

    if (tok = consume_ident()) {
        if (consume("(")) {
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = strndup(tok->str, tok->len);
            node->args = func_args();

            VarScope *sc = find_var(tok);
            if (sc) {
                if (!sc->var || sc->var->ty->kind != TY_FUNC)
                    error_tok(tok, "not a function");
                node->ty = sc->var->ty->return_ty;
            } else {
                node->ty = int_type();
            }
            return node;
        }

        VarScope *sc = find_var(tok);
        if (sc) {
            if (sc->var)
                return new_var(sc->var, tok);
            if (sc->enum_ty)
                return new_num(sc->enum_val, tok);
        }
        error_tok(tok, "undefined variable");
    }

    tok = token;
    if (tok->kind == TK_STR) {
        token = token->next;

        Type *ty = array_of(char_type(), tok->cont_len);
        Var *var = push_var(new_label(), ty, false, NULL);
        var->initializer = gvar_init_string(tok->contents, tok->cont_len);
        return new_var(var, tok);
    }

    if (tok->kind != TK_NUM)
        error_tok(tok, "expected expression");
    return new_num(expect_number(), tok);
}