#ifndef SRCLANG_FUNCTION_H
#define SRCLANG_FUNCTION_H

#include <memory>

#include "DebugInfo.h"
#include "Instructions.h"
#include "Value.h"

namespace SrcLang {

    enum class FunctionType {
        Function,
        Method,
        Initializer,
        Native
    };

    struct Function {
        FunctionType type{FunctionType::Function};
        std::string id;
        std::unique_ptr<Instructions> instructions{nullptr};
        int nlocals{0};
        int nparams{0};
        bool is_variadic{false};
        std::shared_ptr<DebugInfo> debug_info{nullptr};
    };

    struct Closure {
        Function *fun;
        std::vector<Value> free{0};
    };

}  // SrcLang

#endif
