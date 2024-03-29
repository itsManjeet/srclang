#ifndef SRCLANG_BUILTIN_H
#define SRCLANG_BUILTIN_H

#include "../Interpreter/MemoryManager/MemoryManager.h"
#include "../Value/Value.h"

namespace srclang {
    struct Interpreter;
    struct Language;

    typedef Value (*Builtin)(std::vector<Value> &, Interpreter *);

#define SRCLANG_BUILTIN(id)                            \
    Value builtin_##id(std::vector<Value> const& args, \
                       Interpreter* interpreter)
#define SRCLANG_BUILTIN_LIST              \
    X(println)                            \
    X(print)                              \
    X(gc)                                 \
    X(len)                                \
    X(append)                             \
    X(range)                              \
    X(clone)                              \
    X(eval)                               \
    X(pop)                                \
    X(call)                               \
    X(alloc)                              \
    X(free)                               \
    X(bound)                              \
    X(exit)                               \
    X(open)                               \
    X(exec)                               \
    X(socket)                             \
    X(use)

    struct Interpreter;
#define X(id) SRCLANG_BUILTIN(id);

    SRCLANG_BUILTIN_LIST

#undef X

    enum Builtins {
#define X(id) BUILTIN_##id,
        SRCLANG_BUILTIN_LIST
#undef X
    };
    extern std::vector<Value> builtins;
}  // namespace srclang

#endif  // SRCLANG_BUILTIN_H
