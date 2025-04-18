#ifndef SRCLANG_SYMBOLTABLE_H
#define SRCLANG_SYMBOLTABLE_H

#include <optional>

#include "Value.h"

namespace SrcLang {

#define SRCLANG_SYMBOL_SCOPE_LIST \
    X(BUILTIN)                    \
    X(GLOBAL)                     \
    X(LOCAL)                      \
    X(FREE)                       \
    X(FUNCTION)                   \
    X(TYPE)

struct Symbol {
    std::string name{};
    enum Scope {
#define X(id) id,
        SRCLANG_SYMBOL_SCOPE_LIST
#undef X
    } scope{GLOBAL};
    int index{0};
};

static const std::vector<std::string> SRCLANG_SYMBOL_ID = {
#define X(id) #id,
    SRCLANG_SYMBOL_SCOPE_LIST
#undef X
};

class SymbolTable {
   public:
    SymbolTable *parent{nullptr};
    std::map<std::string, Symbol> store;
    std::vector<Symbol> free;
    int definitions{0};

   public:
    explicit SymbolTable(SymbolTable *parent = nullptr);

    Symbol define(const std::string &name);

    Symbol define(const std::string &name, int index);

    Symbol define(const Symbol &other);

    Symbol defineFun(const std::string &name);

    std::optional<Symbol> resolve(const std::string &name);
};
}  // namespace SrcLang

#endif  // SRCLANG_SYMBOLTABLE_H
