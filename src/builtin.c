#include "_priv.h"

#define BUILTIN_ID(id) builtin_##id
#define BUILTIN_FUN(id) static Capsule BUILTIN_ID(id)(Capsule args, Capsule scope)

BUILTIN_FUN(car) {
    if (CAPSULE_NILP(args) || !CAPSULE_NILP(CAPSULE_CDR(args)))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    if (CAPSULE_NILP(CAPSULE_CAR(args))) {
        return Capsule_nil;
    } else if (CAPSULE_CAR(args).type != CapsuleType_Pair)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return CAPSULE_CAR(CAPSULE_CAR(args));
}

BUILTIN_FUN(cdr) {
    if (CAPSULE_NILP(args) || !CAPSULE_NILP(CAPSULE_CDR(args)))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    if (CAPSULE_NILP(CAPSULE_CAR(args))) {
        return Capsule_nil;
    } else if (CAPSULE_CAR(args).type != CapsuleType_Pair)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return CAPSULE_CDR(CAPSULE_CAR(args));
}

BUILTIN_FUN(cons) {
    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    return Capsule_cons(CAPSULE_CAR(args), CAPSULE_CAR(CAPSULE_CDR(args)));
}

BUILTIN_FUN(eq) {
    Capsule a, b;
    int eq;

    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    a = CAPSULE_CAR(args);
    b = CAPSULE_CAR(CAPSULE_CDR(args));

    if (a.type == b.type) {
        switch (a.type) {
            case CapsuleType_Nil:
                eq = 1;
                break;
            case CapsuleType_Pair:
            case CapsuleType_Closure:
            case CapsuleType_Macro:
                eq = (a.as.pair == b.as.pair);
                break;
            case CapsuleType_Symbol:
                eq = (a.as.symbol == b.as.symbol);
                break;
            case CapsuleType_Integer:
                eq = (a.as.integer == b.as.integer);
                break;
            case CapsuleType_Builtin:
                eq = (a.as.builtin == b.as.builtin);
                break;
        }
    } else {
        eq = 0;
    }

    return eq ? CAPSULE_SYMBOL("T") : Capsule_nil;
}

BUILTIN_FUN(pairp) {
    if (CAPSULE_NILP(args) || !CAPSULE_NILP(CAPSULE_CDR(args)))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    return (CAPSULE_CAR(args).type == CapsuleType_Pair) ? CAPSULE_SYMBOL("T") : Capsule_nil;
}

BUILTIN_FUN(procp) {
    if (CAPSULE_NILP(args) || !CAPSULE_NILP(CAPSULE_CDR(args)))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    return (CAPSULE_CAR(args).type == CapsuleType_Builtin || CAPSULE_CAR(args).type == CapsuleType_Closure) ? CAPSULE_SYMBOL("T") : Capsule_nil;
}

BUILTIN_FUN(add) {
    Capsule a, b;

    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    a = CAPSULE_CAR(args);
    b = CAPSULE_CAR(CAPSULE_CDR(args));

    if (a.type != CapsuleType_Integer || b.type != CapsuleType_Integer)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return CAPSULE_INT(a.as.integer + b.as.integer);
}

BUILTIN_FUN(sub) {
    Capsule a, b;

    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    a = CAPSULE_CAR(args);
    b = CAPSULE_CAR(CAPSULE_CDR(args));

    if (a.type != CapsuleType_Integer || b.type != CapsuleType_Integer)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return CAPSULE_INT(a.as.integer - b.as.integer);
}

BUILTIN_FUN(mul) {
    Capsule a, b;

    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    a = CAPSULE_CAR(args);
    b = CAPSULE_CAR(CAPSULE_CDR(args));

    if (a.type != CapsuleType_Integer || b.type != CapsuleType_Integer)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return CAPSULE_INT(a.as.integer * b.as.integer);
}

BUILTIN_FUN(div) {
    Capsule a, b;

    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    a = CAPSULE_CAR(args);
    b = CAPSULE_CAR(CAPSULE_CDR(args));

    if (a.type != CapsuleType_Integer || b.type != CapsuleType_Integer)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return CAPSULE_INT(a.as.integer / b.as.integer);
}

BUILTIN_FUN(less) {
    Capsule a, b;

    if (CAPSULE_NILP(args) || CAPSULE_NILP(CAPSULE_CDR(args)) || !CAPSULE_NILP(CAPSULE_CDR(CAPSULE_CDR(args))))
        return CAPSULE_ERROR(CapsuleError_InvalidArgs);

    a = CAPSULE_CAR(args);
    b = CAPSULE_CAR(CAPSULE_CDR(args));

    if (a.type != CapsuleType_Integer || b.type != CapsuleType_Integer)
        return CAPSULE_ERROR(CapsuleError_InvalidType);

    return (a.as.integer < b.as.integer) ? CAPSULE_SYMBOL("T") : Capsule_nil;
}

void define_builtins(Capsule scope) {
#define DEFINE_CONST(id, val) Capsule_scope_define(scope, CAPSULE_SYMBOL(id), val);
#define DEFINE_BUILTIN(id, fun) Capsule_scope_define(scope, CAPSULE_SYMBOL(id), CAPSULE_BUILTIN(BUILTIN_ID(fun)));

    DEFINE_BUILTIN("CAR", car);
    DEFINE_BUILTIN("CDR", cdr);
    DEFINE_BUILTIN("CONS", cons);
    DEFINE_BUILTIN("+", add);
    DEFINE_BUILTIN("-", sub);
    DEFINE_BUILTIN("*", mul);
    DEFINE_BUILTIN("/", div);
    DEFINE_CONST("T", CAPSULE_SYMBOL("T"));
    DEFINE_BUILTIN("<", less);
    DEFINE_BUILTIN("EQ?", eq);
    DEFINE_BUILTIN("PAIR?", pairp);
    DEFINE_BUILTIN("PROCEDURE?", procp);
}