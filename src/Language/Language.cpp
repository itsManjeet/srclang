#include "Language.h"

#include <fstream>
#include <utility>
#include "../Compiler/SymbolTable/SymbolTable.h"
#include "../Compiler/Compiler.h"
#include "../Interpreter/Interpreter.h"

using namespace srclang;

Language::Language()
        : globals(65536), options(
        {
                {"VERSION", SRCLANG_VERSION},
                {"GC_HEAP_GROW_FACTOR",   1.3f},
                {"GC_INITIAL_TRIGGER",    10},
                {"SEARCH_PATH",           ""},
                {"IR",                    false},
                {"EXPERIMENTAL_FEATURES", false},
                {"DEBUG",                 false},
                {"BREAK",                 false},
        }) {
    memoryManager = new MemoryManager();
    symbolTable = new SymbolTable();

    for (auto b: builtins) {
        memoryManager->heap.push_back(b);
    }
    {
        int i = 0;
#define X(id) symbolTable->define(#id, i++);
        SRCLANG_BUILTIN_LIST
#undef X
    }

    symbolTable->define("__FILE__");

    define("true", SRCLANG_VALUE_TRUE);
    define("false", SRCLANG_VALUE_FALSE);
    define("null", SRCLANG_VALUE_NULL);

}

Language::~Language() {
    delete memoryManager;
    delete symbolTable;
}

void Language::define(const std::string &id, Value value) {
    auto symbol = symbolTable->resolve(id);
    if (symbol == std::nullopt) {
        symbol = symbolTable->define(id);
    }
    globals[symbol->index] = value;
    registerObject(value);
}

Value Language::registerObject(Value value) const {
    memoryManager->heap.push_back(value);
    return value;
}

std::tuple<Value, ByteCode, std::shared_ptr<DebugInfo>>
Language::compile(std::string const &input, std::string const &filename) {
    std::tuple<Value, ByteCode, std::shared_ptr<DebugInfo>> ret;

    auto compiler = Compiler(input.begin(), input.end(), filename, this);
    try {
        compiler.compile();
        std::get<0>(ret) = SRCLANG_VALUE_TRUE;
        std::get<1>(ret) = std::move(compiler.code());
        std::get<2>(ret) = compiler.debugInfo();

        if (std::get<bool>(options["IR"])) {
            std::cout << std::get<1>(ret) << std::endl;
        }
    } catch (const std::exception &exception) {
        std::get<0>(ret) = registerObject(SRCLANG_VALUE_ERROR(strdup(exception.what())));
    }

    return ret;
}

Value Language::execute(const std::string &input, const std::string &filename) {
    auto [status, code, debug_info] = compile(input, filename);
    if (status != SRCLANG_VALUE_TRUE) {
        return status;
    }
    return execute(code, debug_info);
}

Value Language::execute(ByteCode &code, const std::shared_ptr<DebugInfo> &debugInfo) {
    auto interpreter = Interpreter(code, debugInfo, this);
    if (!interpreter.run()) {
        return registerObject(SRCLANG_VALUE_ERROR(strdup(interpreter.getError().c_str())));
    }
    return *interpreter.sp;
}

Value Language::execute(const std::filesystem::path &filename) {
    if (!std::filesystem::exists(filename)) {
        return SRCLANG_VALUE_ERROR(strdup(("Missing file " + filename.string()).c_str()));
    }
    std::ifstream reader(filename);

    std::string input((std::istreambuf_iterator<char>(reader)), (std::istreambuf_iterator<char>()));
    return execute(input, filename.string());
}

void Language::appendSearchPath(const std::string &path) {
    options["SEARCH_PATH"] = path + ":" + std::get<std::string>(options["SEARCH_PATH"]);
}

Value Language::resolve(const std::string &id) {
    auto symbol = symbolTable->resolve(id);
    if (symbol == std::nullopt) {
        return SRCLANG_VALUE_NULL;
    }
    switch (symbol->scope) {
        case Symbol::GLOBAL:
            return globals[symbol->index];
        default:
            return SRCLANG_VALUE_NULL;
    }
}


Value Language::call(Value callee, const std::vector<Value> &args) {
    ByteCode code;
    code.instructions = std::make_unique<Instructions>();
    code.constants = this->constants;

    code.constants.push_back(callee);
    code.instructions->push_back(static_cast<const unsigned int>(OpCode::CONST_));
    code.instructions->push_back(code.constants.size() - 1);

    for (auto arg: args) {
        code.constants.push_back(arg);
        code.instructions->push_back(static_cast<const unsigned int>(OpCode::CONST_));
        code.instructions->push_back(code.constants.size() - 1);
    }

    code.instructions->push_back(static_cast<const unsigned int>(OpCode::CALL));
    code.instructions->push_back(args.size());
    code.instructions->push_back(static_cast<const unsigned int>(OpCode::HLT));

    this->constants = code.constants;

    auto interpreter = Interpreter(code, nullptr, this);
    if (!interpreter.run()) {
        return SRCLANG_VALUE_ERROR(strdup("INTERPRETATION FAILED"));
    }
    if (std::distance(interpreter.stack.begin(), interpreter.sp) > 0) {
        return *(interpreter.sp - 1);
    }
    return SRCLANG_VALUE_TRUE;
}
