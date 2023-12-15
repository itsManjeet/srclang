/*
 * Copyright (c) 2023 Manjeet Singh <itsmanjeet1998@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>


typedef struct Atom Atom;

typedef enum Result {
    RESULT_OK = 0,
    RESULT_ERROR_SYNTAX,
    RESULT_ERROR_UNBOUND,
    RESULT_ERROR_ARGS,
    RESULT_ERROR_TYPE,
} Result;

typedef Result (*Builtin)(Atom args, Atom *result);

struct Atom {
    enum {
        ATOM_TYPE_NIL,
        ATOM_TYPE_PAIR,
        ATOM_TYPE_SYMBOL,
        ATOM_TYPE_INTEGER,
        ATOM_TYPE_BUILTIN,
        ATOM_TYPE_CLOSURE,
        ATOM_TYPE_MACRO,
    } type;
    union {
        struct Pair *pair;
        const char *symbol;
        long integer;
        Builtin builtin;
    } as;
};

typedef struct Pair {
    Atom atom[2];
} Pair;


#define CAR(p) ((p).as.pair->atom[0])
#define CDR(p) ((p).as.pair->atom[1])
#define CONS(car, cdr) cons_((car), (cdr))
#define INTEGER(x) ((Atom){.type = ATOM_TYPE_INTEGER, .as.integer = (x)})
#define BUILTIN(b) ((Atom){.type = ATOM_TYPE_BUILTIN, .as.builtin = (b)})
#define SYMBOL(p) symbol_((p))
#define CLOSURE(e, a, b, r) closure_((e), (a), (b), (r))


#define IS_NIL(atom) ((atom).type == ATOM_TYPE_NIL)

static const Atom nil = {ATOM_TYPE_NIL};

static Atom global = {ATOM_TYPE_NIL};


static Atom cons_(Atom car, Atom cdr) {
    Atom p = {ATOM_TYPE_PAIR};
    p.as.pair = malloc(sizeof(Pair));
    CAR(p) = car;
    CDR(p) = cdr;
    return p;
}

static Atom symbol_(const char *s) {
    Atom a, p;
    p = global;
    while (!IS_NIL(p)) {
        a = CAR(p);
        if (strcmp(a.as.symbol, s) == 0) {
            return a;
        }
        p = CDR(p);
    }
    a.type = ATOM_TYPE_SYMBOL;
    a.as.symbol = strdup(s);

    global = CONS(a, global);
    return a;
}

static int is_proper_list(Atom expr) {
    while (!IS_NIL(expr)) {
        if (expr.type != ATOM_TYPE_PAIR) {
            return 0;
        }
        expr = CDR(expr);
    }
    return 1;
}


static Result closure_(Atom env, Atom args, Atom body, Atom *output) {
    Atom p;
    if (!is_proper_list(body)) return RESULT_ERROR_SYNTAX;

    p = args;
    while (!IS_NIL(p)) {
        if (p.type == ATOM_TYPE_SYMBOL) break;
        else if (p.type != ATOM_TYPE_PAIR ||
                 CAR(p).type != ATOM_TYPE_SYMBOL)
            return RESULT_ERROR_TYPE;
        p = CDR(p);
    }
    *output = CONS(env, CONS(args, body));
    output->type = ATOM_TYPE_CLOSURE;
    return RESULT_OK;
}

static Atom copy(Atom list) {
    Atom a, p;
    if (IS_NIL(list)) return nil;

    a = CONS(CAR(list), nil);
    p = a;
    list = CDR(list);
    while (!IS_NIL(list)) {
        CDR(p) = CONS(CAR(list), nil);
        p = CDR(p);
        list = CDR(list);
    }
    return a;
}


static Atom env_create(Atom parent) {
    return CONS(parent, nil);
}

// (parent (identifier . value)...)
static Result env_get(Atom env, Atom symbol, Atom *output) {
    Atom parent = CAR(env);
    Atom bindings = CDR(env);

    while (!IS_NIL(bindings)) {
        Atom binding = CAR(bindings);
        if (CAR(binding).as.symbol == symbol.as.symbol) {
            *output = CDR(binding);
            return RESULT_OK;
        }
        bindings = CDR(bindings);
    }
    if (IS_NIL(parent)) return RESULT_ERROR_UNBOUND;
    return env_get(parent, symbol, output);
}

static Result env_set(Atom env, Atom symbol, Atom value) {
    Atom bindings = CDR(env);
    Atom binding = nil;

    while (!IS_NIL(bindings)) {
        binding = CAR(bindings);
        if (CAR(binding).as.symbol == symbol.as.symbol) {
            CDR(binding) = value;
            return RESULT_OK;
        }
        bindings = CDR(bindings);
    }
    binding = CONS(symbol, value);
    CDR(env) = CONS(binding, CDR(env));
    return RESULT_OK;
}


static void print(Atom atom, FILE *out) {
    switch (atom.type) {
        case ATOM_TYPE_NIL:
            fprintf(out, "NIL");
            break;
        case ATOM_TYPE_PAIR:
            fprintf(out, "(");
            print(CAR(atom), out);
            atom = CDR(atom);
            while (!IS_NIL(atom)) {
                if (atom.type == ATOM_TYPE_PAIR) {
                    fprintf(out, " ");
                    print(CAR(atom), out);
                    atom = CDR(atom);
                } else {
                    fprintf(out, " . ");
                    print(atom, out);
                    break;
                }
            }
            fprintf(out, ")");
            break;
        case ATOM_TYPE_SYMBOL:
            fprintf(out, "%s", atom.as.symbol);
            break;
        case ATOM_TYPE_INTEGER:
            fprintf(out, "%ld", atom.as.integer);
            break;
        case ATOM_TYPE_BUILTIN:
            fprintf(out, "#<BUILTIN:%p>", atom.as.builtin);
            break;

    }
}

static Result lex(const char *str, const char **start, const char **end) {
    const char *ws = " \t\n";
    const char *delim = "() \t\n";
    const char *prefix = "()\'";

    str += strspn(str, ws);
    if (str[0] == '\0') {
        *start = *end = NULL;
        return RESULT_ERROR_SYNTAX;
    }
    *start = str;
    if (strchr(prefix, str[0]) != NULL) *end = str + 1;
    else *end = str + strcspn(str, delim);

    return RESULT_OK;
}

static Result expr(const char *input, const char **end, Atom *output);


static Result simple(const char *start, const char *end, Atom *output) {
    char *buf, *p;
    long value = strtol(start, &p, 10);
    if (p == end) {
        output->type = ATOM_TYPE_INTEGER;
        output->as.integer = value;

        return RESULT_OK;
    }

    buf = malloc(end - start + 1);
    p = buf;
    while (start != end) *p++ = (char) toupper(*start), ++start;
    *p = '\0';

    if (strcmp(buf, "NIL") == 0) *output = nil;
    else *output = SYMBOL(buf);

    free(buf);

    return RESULT_OK;
}

static Result list(const char *start, const char **end, Atom *output) {
    Atom p;
    *end = start;
    p = *output = nil;

    for (;;) {
        const char *token;
        Atom item;
        Result result = lex(*end, &token, end);
        if (result) return result;
        if (token[0] == ')') return RESULT_OK;

        if (token[0] == '.' && *end - token == 1) {
            if (IS_NIL(p)) return RESULT_ERROR_SYNTAX;
            result = expr(*end, end, &item);
            if (result) return result;

            CDR(p) = item;
            result = lex(*end, &token, end);
            if (!result && token[0] != ')') result = RESULT_ERROR_SYNTAX;
            return result;
        }

        result = expr(token, end, &item);
        if (result) return result;

        if (IS_NIL(p)) {
            *output = CONS(item, nil);
            p = *output;
        } else {
            CDR(p) = CONS(item, nil);
            p = CDR(p);
        };
    }
}

Result expr(const char *input, const char **end, Atom *output) {
    const char *token;

    Result result = lex(input, &token, end);
    if (result) return result;

    if (token[0] == '(') return list(*end, end, output);
    if (token[0] == ')') return RESULT_ERROR_SYNTAX;
    if (token[0] == '\'') {
        *output = CONS(SYMBOL("QUOTE"), CONS(nil, nil));
        return expr(*end, end, &CAR(CDR(*output)));
    }

    return simple(token, *end, output);
}

static Result eval(Atom expr, Atom env, Atom *output);

static Result apply(Atom fun, Atom args, Atom *output) {
    if (fun.type == ATOM_TYPE_BUILTIN) return (*fun.as.builtin)(args, output);
    else if (fun.type != ATOM_TYPE_CLOSURE) return RESULT_ERROR_TYPE;

    Atom env = env_create(CAR(fun));
    Atom arg_names = CAR(CDR(fun));
    Atom body = CDR(CDR(fun));

    while (!IS_NIL(arg_names)) {
        if (arg_names.type == ATOM_TYPE_SYMBOL) {
            env_set(env, arg_names, args);
            args = nil;
            break;
        }
        if (IS_NIL(args)) {
            return RESULT_ERROR_ARGS;
        }
        env_set(env, CAR(arg_names), CAR(args));
        arg_names = CDR(arg_names);
        args = CDR(args);
    }
    if (!IS_NIL(args)) return RESULT_ERROR_ARGS;

    while (!IS_NIL(body)) {
        Result result = eval(CAR(body), env, output);
        if (result) return result;
        body = CDR(body);
    }
    return RESULT_OK;
}


Result eval(Atom expr, Atom env, Atom *output) {
    Atom op, args, p;
    Result result;

    if (expr.type == ATOM_TYPE_SYMBOL) {
        return env_get(env, expr, output);
    } else if (expr.type != ATOM_TYPE_PAIR) {
        *output = expr;
        return RESULT_OK;
    }
    if (!is_proper_list(expr)) return RESULT_ERROR_SYNTAX;

    op = CAR(expr);
    args = CDR(expr);

    if (op.type == ATOM_TYPE_SYMBOL) {
        if (strcmp(op.as.symbol, "QUOTE") == 0) { // (quote expr)
            if (IS_NIL(args) || !IS_NIL(CDR(args))) {
                return RESULT_ERROR_ARGS;
            }
            *output = CAR(args);
            return RESULT_OK;
        } else if (strcmp(op.as.symbol, "DEF") == 0) { // (def sym val)
            Atom sym, val;

            if (IS_NIL(args) || IS_NIL(CDR(args))) {
                return RESULT_ERROR_ARGS;
            }

            sym = CAR(args);
            if (sym.type == ATOM_TYPE_PAIR) {
                result = CLOSURE(env, CDR(sym), CDR(args), &val);
                sym = CAR(sym);
                if (sym.type != ATOM_TYPE_SYMBOL) return RESULT_ERROR_TYPE;

            } else if (sym.type != ATOM_TYPE_SYMBOL) {
                if (!IS_NIL(CDR(CDR(args)))) return RESULT_ERROR_ARGS;
                result = eval(CDR(CDR(args)), env, &val);
            } else {
                return RESULT_ERROR_TYPE;
            }
            if (result) return result;

            *output = sym;
            return env_set(env, sym, val);
        } else if (strcmp(op.as.symbol, "LAMBDA") == 0) {
            if (IS_NIL(args) || IS_NIL(CDR(args))) return RESULT_ERROR_ARGS;
            return CLOSURE(env, CAR(args), CDR(args), output);
        } else if (strcmp(op.as.symbol, "IF") == 0) { // (if test truth else)
            Atom cond, val;
            if (IS_NIL(args) || IS_NIL(CDR(args)) || IS_NIL(CDR(CDR(args))) ||
                !IS_NIL(CDR(CDR(CDR(args)))))
                return RESULT_ERROR_ARGS;
            result = eval(CAR(args), env, &cond);
            if (result) return result;

            val = IS_NIL(cond) ? CAR(CDR(CDR(args))) : CAR(CDR(args));
            return eval(val, env, output);
        } else if (strcmp(op.as.symbol, "MACRO") == 0) {
            Atom name, macro;
            if (IS_NIL(args) || IS_NIL(CDR(args))) return RESULT_ERROR_ARGS;
            if (CAR(args).type != ATOM_TYPE_PAIR) return RESULT_ERROR_SYNTAX;

            name = CAR(CAR(args));
            if (name.type != ATOM_TYPE_SYMBOL) return RESULT_ERROR_TYPE;

            result = CLOSURE(env, CDR(CAR(args)), CDR(args), &macro);
            if (result) return result;

            macro.type = ATOM_TYPE_MACRO;
            *output = name;
            return env_set(env, name, macro);
        }
    }
    result = eval(op, env, &op);
    if (result) return result;

    if (op.type == ATOM_TYPE_MACRO) {
        Atom expansion;
        op.type = ATOM_TYPE_CLOSURE;
        result = apply(op, args, &expansion);
        if (result) return result;

        return eval(expansion, env, output);
    }

    args = copy(args);
    p = args;
    while (!IS_NIL(p)) {
        result = eval(CAR(p), env, &CAR(p));
        if (result) return result;
        p = CDR(p);
    }
    return apply(op, args, output);
}

static Result builtin_car(Atom args, Atom *output) {
    if (IS_NIL(args) || !IS_NIL(CDR(args))) return RESULT_ERROR_ARGS;
    if (IS_NIL(CAR(args))) *output = nil;
    else if (CAR(args).type != ATOM_TYPE_PAIR) return RESULT_ERROR_TYPE;
    else *output = CAR(CAR(args));
    return RESULT_OK;
}

static Result builtin_cdr(Atom args, Atom *output) {
    if (IS_NIL(args) || !IS_NIL(CDR(args))) return RESULT_ERROR_ARGS;

    if (IS_NIL(CAR(args))) *output = nil;
    else if (CAR(args).type != ATOM_TYPE_PAIR) return RESULT_ERROR_TYPE;
    else *output = CDR(CAR(args));
    return RESULT_OK;
}

static Result builtin_cons(Atom args, Atom *output) {
    if (IS_NIL(args) || IS_NIL(CDR(args)) || !IS_NIL(CDR(CDR(args)))) return RESULT_ERROR_ARGS;

    *output = CONS(CAR(args), CAR(CDR(args)));
    return RESULT_OK;
}

#define OPERATOR_INT(ty, op) \
static Result builtin_##ty(Atom args, Atom *output) { \
    Atom a, b; \
    if (IS_NIL(args) || IS_NIL(CDR(args)) || !IS_NIL(CDR(CDR(args)))) { \
        return RESULT_ERROR_ARGS; \
    } \
    a = CAR(args); \
    b = CAR(CDR(args)); \
    if (a.type != ATOM_TYPE_INTEGER || b.type != ATOM_TYPE_INTEGER) \
        return RESULT_ERROR_TYPE; \
    *output = INTEGER(a.as.integer op b.as.integer); \
    return RESULT_OK; \
}

OPERATOR_INT(add, +)

OPERATOR_INT(sub, -)

OPERATOR_INT(mul, *)

OPERATOR_INT(div, /)

#define OPERATOR_INT_COMP(ty, op) \
static Result builtin_##ty(Atom args, Atom *output) { \
    Atom a, b; \
    if (IS_NIL(args) || IS_NIL(CDR(args)) || !IS_NIL(CDR(CDR(args)))) { \
        return RESULT_ERROR_ARGS; \
    } \
    a = CAR(args); \
    b = CAR(CDR(args)); \
    if (a.type != ATOM_TYPE_INTEGER || b.type != ATOM_TYPE_INTEGER) \
        return RESULT_ERROR_TYPE; \
    *output = a.as.integer op b.as.integer ? SYMBOL("T") : nil; \
    return RESULT_OK; \
}

OPERATOR_INT_COMP(eq, ==)

OPERATOR_INT_COMP(lt, <)

static char *read_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) return NULL;

    fread(buffer, sizeof(char), size, file);
    buffer[size] = '\0';
    fclose(file);

    return buffer;
}

void load_file(Atom env, const char *path) {
    char *data = read_file(path);
    if (data) {
        const char *p = data;
        Atom output;
        while (expr(p, &p, &output) == RESULT_OK) {
            Atom result;
            if (eval(output, env, &result)) {
                fprintf(stderr, "Error in expression:\n\t");
                print(output, stderr);
                fprintf(stderr, "\n");
            } else {
                print(result, stdout);
                fputc('\n', stdout);
            }
        }
        free(data);

    }
}


int main(int argc, char **argv) {
    char *input = NULL;
    Atom env = env_create(nil);

    env_set(env, SYMBOL("CAR"), BUILTIN(builtin_car));
    env_set(env, SYMBOL("CDR"), BUILTIN(builtin_cdr));
    env_set(env, SYMBOL("CONS"), BUILTIN(builtin_cons));
    env_set(env, SYMBOL("+"), BUILTIN(builtin_add));
    env_set(env, SYMBOL("-"), BUILTIN(builtin_sub));
    env_set(env, SYMBOL("*"), BUILTIN(builtin_mul));
    env_set(env, SYMBOL("/"), BUILTIN(builtin_div));
    env_set(env, SYMBOL("="), BUILTIN(builtin_eq));
    env_set(env, SYMBOL("<"), BUILTIN(builtin_lt));
    env_set(env, SYMBOL("T"), SYMBOL("T"));

    load_file(env, "lib.src");

    while ((input = readline("> ")) != NULL) {
        const char *p = input;
        Atom expr_value, output;
        Result result = expr(p, &p, &expr_value);
        if (!result) result = eval(expr_value, env, &output);

        switch (result) {
            case RESULT_OK:
                print(output, stdout);
                printf("\n");
                break;
            case RESULT_ERROR_SYNTAX:
                fprintf(stderr, "Error: Syntax error");
                break;
            case RESULT_ERROR_UNBOUND:
                fprintf(stderr, "Error: Symbol not bound");
                break;
            case RESULT_ERROR_ARGS:
                fprintf(stderr, "Error: Wrong number of arguments");
                break;
            case RESULT_ERROR_TYPE:
                fprintf(stderr, "Error: Wrong type");
                break;
        }
        free(input);
    }
    return 0;
}