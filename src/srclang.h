#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Type Type;
typedef struct Member Member;
typedef struct Initializer Initializer;

typedef enum {
    TK_RESERVED,
    TK_IDENT,
    TK_STR,
    TK_NUM,
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    long val;
    char *str;
    int len;

    char *contents;
    char cont_len;
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
void warn_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
Token *consume_ident();
void expect(char *op);
long expect_number();
char *expect_ident();
bool at_eof();
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
Token *tokenize();

extern char *filename;
extern char *user_input;
extern Token *token;

typedef struct Var Var;
struct Var {
    char *name;
    Type *ty;
    Token *tok;
    bool is_local;

    int offset;

    Initializer *initializer;
};

typedef struct VarList VarList;
struct VarList {
    VarList *next;
    Var *var;
};

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_BITAND,
    ND_BITOR,
    ND_BITXOR,
    ND_SHL,
    ND_SHR,
    ND_EQ,
    ND_NE,
    ND_LT,
    ND_LE,
    ND_ASSIGN,
    ND_TERNARY,
    ND_PRE_INC,
    ND_PRE_DEC,
    ND_POST_INC,
    ND_POST_DEC,
    ND_A_ADD,
    ND_A_SUB,
    ND_A_MUL,
    ND_A_DIV,
    ND_A_SHL,
    ND_A_SHR,
    ND_COMMA,
    ND_MEMBER,
    ND_ADDR,
    ND_DEREF,
    ND_NOT,
    ND_BITNOT,
    ND_LOGAND,
    ND_LOGOR,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_SWITCH,
    ND_CASE,
    ND_SIZEOF,
    ND_BLOCK,
    ND_BREAK,
    ND_CONTINUE,
    ND_GOTO,
    ND_LABEL,
    ND_FUNCALL,
    ND_EXPR_STMT,
    ND_STMT_EXPR,
    ND_VAR,
    ND_NUM,
    ND_CAST,
    ND_NULL,
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *next;
    Type *ty;
    Token *tok;

    Node *lhs;
    Node *rhs;

    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    Node *body;

    char *member_name;
    Member *member;

    char *funcname;
    Node *args;

    char *label_name;

    Node *case_next;
    Node *default_case;
    int case_label;
    int case_end_label;

    Var *var;
    long val;
};

struct Initializer {
    Initializer *next;

    int sz;
    long val;

    char *label;
    long addend;
};

typedef struct Function Function;
struct Function {
    Function *next;
    char *name;
    VarList *params;
    bool pub;

    Type *ret_type;

    Node *node;
    VarList *locals;
    int stack_size;
};

typedef struct {
    VarList *globals;
    Function *fns;
} Program;

Program *program();

typedef enum {
    TY_VOID,
    TY_BOOL,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    TY_LONG,
    TY_ENUM,
    TY_PTR,
    TY_ARRAY,
    TY_STRUCT,
    TY_FUNC,
} TypeKind;

struct Type {
    TypeKind kind;
    bool is_static;
    bool is_incomplete;
    int align;
    Type *base;
    int array_size;
    Member *members;
    Type *return_ty;
};

struct Member {
    Member *next;
    Type *ty;
    Token *tok;
    char *name;
    int offset;
};

int align_to(int n, int align);
Type *void_type();
Type *bool_type();
Type *char_type();
Type *short_type();
Type *int_type();
Type *long_type();
Type *enum_type();
Type *struct_type();
Type *func_type(Type *return_ty);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
int size_of(Type *ty, Token *tok);

void add_type(Program *prog);

void codegen(Program *prog);