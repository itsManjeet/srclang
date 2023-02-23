#include <dlfcn.h>
#include <ffi.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
using namespace std;

#define SRCLANG_VERSION 20230221

using Iterator = string::iterator;
using Byte = uint8_t;

#define SRCLANG_TOKEN_TYPE_LIST \
    X(Reserved)                 \
    X(Identifier)               \
    X(String)                   \
    X(Number)                   \
    X(Eof)

enum class TokenType : uint8_t {
#define X(id) id,
    SRCLANG_TOKEN_TYPE_LIST
#undef X
};

const vector<string> SRCLANG_TOKEN_ID = {
#define X(id) #id,
    SRCLANG_TOKEN_TYPE_LIST
#undef X
};

template <typename Iterator>
struct Token {
    TokenType type;
    string literal;
    Iterator pos;

    friend ostream& operator<<(ostream& os, const Token token) {
        os << SRCLANG_TOKEN_ID[static_cast<int>(token.type)] << ":"
           << token.literal;
        return os;
    }
};

#define SRCLANG_OPCODE_LIST \
    X(CONST)                \
    X(LOAD)                 \
    X(STORE)                \
    X(ADD)                  \
    X(SUB)                  \
    X(MUL)                  \
    X(DIV)                  \
    X(NEG)                  \
    X(NOT)                  \
    X(COMMAND)              \
    X(EQ)                   \
    X(NE)                   \
    X(LT)                   \
    X(LE)                   \
    X(GT)                   \
    X(GE)                   \
    X(BREAK)                \
    X(CONTINUE)             \
    X(FUN)                  \
    X(CALL)                 \
    X(POP)                  \
    X(RET)                  \
    X(JNZ)                  \
    X(JMP)                  \
    X(HLT)

enum class OpCode : uint8_t {
#define X(id) id,
    SRCLANG_OPCODE_LIST
#undef X
};

const vector<string> SRCLANG_OPCODE_ID = {
#define X(id) #id,
    SRCLANG_OPCODE_LIST
#undef X
};

#define SRCLANG_SYMBOL_SCOPE_LIST \
    X(BUILTIN)                    \
    X(GLOBAL)                     \
    X(LOCAL)                      \
    X(TYPE)

struct Symbol {
    string name;
    enum Scope {
#define X(id) id,
        SRCLANG_SYMBOL_SCOPE_LIST
#undef X
    } scope;
    int index;
};

const vector<string> SRCLANG_SYMBOL_ID = {
#define X(id) #id,
    SRCLANG_SYMBOL_SCOPE_LIST
#undef X
};

template <typename Byte>
struct Instructions : vector<Byte> {
    Instructions() = default;

    size_t emit() { return 0; }

    template <typename T, typename... Types>
    size_t emit(T byte, Types... operand) {
        size_t pos = this->size();
        this->push_back(static_cast<Byte>(byte));
        emit(operand...);
        return pos;
    }
};

#define SRCLANG_VALUE_TYPE_LIST \
    X(Null, "null_t")           \
    X(Boolean, "bool")          \
    X(Decimal, "float")         \
    X(Integer, "int")           \
    X(String, "str")            \
    X(List, "list")             \
    X(Map, "map")               \
    X(Function, "function")     \
    X(Builtin, "builtin")       \
    X(Native, "native")         \
    X(Error, "error")           \
    X(Type, "type")

enum class ValueType : uint8_t {
#define X(id, name) id,
    SRCLANG_VALUE_TYPE_LIST
#undef X
};

static const vector<string> SRCLANG_VALUE_TYPE_ID = {
#define X(id, name) name,
    SRCLANG_VALUE_TYPE_LIST
#undef X
};

typedef uint64_t Value;

#define SRCLANG_VALUE_SIGN_BIT ((uint64_t)0x8000000000000000)
#define SRCLANG_VALUE_QNAN ((uint64_t)0x7ffc000000000000)

#define SRCLANG_VALUE_TAG_NULL 1
#define SRCLANG_VALUE_TAG_FALSE 2
#define SRCLANG_VALUE_TAG_TRUE 3
#define SRCLANG_VALUE_TAG_INT 4
#define SRCLANG_VALUE_TAG_TYPE 5
#define SRCLANG_VALUE_TAG_2 6
#define SRCLANG_VALUE_TAG_3 7

#define SRCLANG_VALUE_IS_BOOL(val) (((val) | 1) == SRCLANG_VALUE_TRUE)
#define SRCLANG_VALUE_IS_NULL(val) ((val) == SRCLANG_VALUE_NULL)
#define SRCLANG_VALUE_IS_DECIMAL(val) \
    (((val)&SRCLANG_VALUE_QNAN) != SRCLANG_VALUE_QNAN)
#define SRCLANG_VALUE_IS_OBJECT(val)                            \
    (((val) & (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_SIGN_BIT)) == \
     (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_SIGN_BIT))

#define SRCLANG_VALUE_IS_INTEGER(val)                          \
    (((val) & (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_INT)) == \
         (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_INT) &&       \
     ((val | SRCLANG_VALUE_SIGN_BIT) != val))

#define SRCLANG_VALUE_IS_TYPE(val)                              \
    (((val) & (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_TYPE)) == \
         (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_TYPE) &&       \
     ((val | SRCLANG_VALUE_SIGN_BIT) != val))

#define SRCLANG_VALUE_AS_BOOL(val) ((val) == SRCLANG_VALUE_TRUE)
#define SRCLANG_VALUE_AS_DECIMAL(val) (srclang_value_to_decimal(val))
#define SRCLANG_VALUE_AS_OBJECT(val)  \
    ((HeapObject*)(uintptr_t)((val) & \
                              ~(SRCLANG_VALUE_SIGN_BIT | SRCLANG_VALUE_QNAN)))
#define SRCLANG_VALUE_AS_TYPE(val) ((ValueType)((val) >> 3))
#define SRCLANG_VALUE_AS_INTEGER(val) ((int)((val) >> 3))

#define SRCLANG_VALUE_BOOL(b) ((b) ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE)
#define SRCLANG_VALUE_DECIMAL(num) (srclang_decimal_to_value(num))
#define SRCLANG_VALUE_OBJECT(obj)                         \
    (Value)(SRCLANG_VALUE_SIGN_BIT | SRCLANG_VALUE_QNAN | \
            (uint64_t)(uintptr_t)(obj))
#define SRCLANG_VALUE_TYPE(ty)                                      \
    ((Value)(SRCLANG_VALUE_QNAN | ((uint64_t)(uint32_t)(ty) << 3) | \
             SRCLANG_VALUE_TAG_TYPE))
#define SRCLANG_VALUE_INTEGER(val)                                   \
    ((Value)(SRCLANG_VALUE_QNAN | ((uint64_t)(uint32_t)(val) << 3) | \
             SRCLANG_VALUE_TAG_INT))

#define SRCLANG_VALUE_HEAP_OBJECT(type, ptr, destructor, printer) \
    SRCLANG_VALUE_OBJECT(                                         \
        (new HeapObject{(type), (ptr), (destructor), (printer)}))

#define SRCLANG_VALUE_STRING(str)                       \
    SRCLANG_VALUE_HEAP_OBJECT(                          \
        ValueType::String, (void*)str, ([](void* ptr) { \
            if (ptr != nullptr) delete[] (char*)ptr;    \
        }),                                             \
        ([](void* ptr) -> string { return (char*)ptr; }))

#define SRCLANG_VALUE_ERROR(err)                  \
    SRCLANG_VALUE_HEAP_OBJECT(                    \
        ValueType::Error, (void*)err,             \
        ([](void* ptr) { delete[] (char*)ptr; }), \
        ([](void* ptr) -> string { return (char*)ptr; }))

#define SRCLANG_VALUE_NATIVE(native)                                          \
    SRCLANG_VALUE_HEAP_OBJECT(ValueType::Native, (void*)native,               \
                              ([](void* ptr) {}), ([](void* ptr) -> string {  \
                                  NativeFunction* native =                    \
                                      reinterpret_cast<NativeFunction*>(ptr); \
                                  return "<native(" + native->id + ")";       \
                              }))

#define SRCLANG_VALUE_BUILTIN(id)                                    \
    SRCLANG_VALUE_HEAP_OBJECT(                                       \
        ValueType::Builtin, (void*)builtin_##id, ([](void* ptr) {}), \
        ([](void* ptr) -> string { return "<builtin " #id ">"; }))

#define SRCLANG_VALUE_FUNCTION(fun)                                           \
    SRCLANG_VALUE_HEAP_OBJECT(                                                \
        ValueType::Function, (void*)fun, ([](void* ptr) {                     \
            delete reinterpret_cast<Function<Byte, Value>*>(ptr);             \
        }),                                                                   \
        ([](void* ptr) -> string {                                            \
            Function<Byte, Value>* fun =                                      \
                reinterpret_cast<Function<Byte, Value>*>(ptr);                \
            vector<Value> constants;                                          \
            stringstream ss;                                                  \
            for (int offset = 0; offset < fun->instructions->size();) {       \
                offset = ByteCode<Byte, Value>::debug(*fun->instructions,     \
                                                      constants, offset, ss); \
                ss << " | ";                                                  \
            }                                                                 \
            return ss.str();                                                  \
        }))

#define SRCLANG_VALUE_TRUE \
    ((Value)(uint64_t)(SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_TRUE))
#define SRCLANG_VALUE_FALSE \
    ((Value)(uint64_t)(SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_FALSE))
#define SRCLANG_VALUE_NULL \
    ((Value)(uint64_t)(SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_NULL))

#define SRCLANG_BUILTIN(id) \
    Value builtin_##id(vector<Value>& args, Interpreter<Byte>* interpreter)

static inline double srclang_value_to_decimal(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value srclang_decimal_to_value(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

struct HeapObject {
    using Destructor = function<void(void*)>;
    using Printer = function<string(void*)>;

    ValueType type;
    void* pointer{nullptr};

    Destructor destructor{nullptr};
    Printer printer{nullptr};

    bool marked{false};
};

struct NativeFunction {
    string id;
    vector<ValueType> param;
    ValueType ret;
};

static const vector<Value> SRCLANG_VALUE_TYPES = {
#define X(id, name) SRCLANG_VALUE_TYPE(ValueType::id),
    SRCLANG_VALUE_TYPE_LIST
#undef X
};

ValueType srclang_value_get_type(Value val) {
    if (SRCLANG_VALUE_IS_NULL(val)) return ValueType::Null;
    if (SRCLANG_VALUE_IS_BOOL(val)) return ValueType::Boolean;
    if (SRCLANG_VALUE_IS_DECIMAL(val)) return ValueType::Decimal;
    if (SRCLANG_VALUE_IS_INTEGER(val)) return ValueType::Integer;
    if (SRCLANG_VALUE_IS_TYPE(val)) return ValueType::Type;
    if (SRCLANG_VALUE_IS_OBJECT(val))
        return (SRCLANG_VALUE_AS_OBJECT(val)->type);
    throw runtime_error("invalid value '" + to_string((uint64_t)val) + "'");
}

string srclang_value_print(Value val) {
    auto type = srclang_value_get_type(val);
    switch (type) {
        case ValueType::Null:
            return "null";
        case ValueType::Boolean:
            return SRCLANG_VALUE_AS_BOOL(val) ? "true" : "false";
        case ValueType::Decimal:
            return to_string(SRCLANG_VALUE_AS_DECIMAL(val));
        case ValueType::Integer:
            return to_string(SRCLANG_VALUE_AS_INTEGER(val));
        case ValueType::Type:
            return "<type(" +
                   SRCLANG_VALUE_TYPE_ID[int(SRCLANG_VALUE_AS_TYPE(val))] +
                   ")>";
        default:
            return SRCLANG_VALUE_AS_OBJECT(val)->printer(
                (SRCLANG_VALUE_AS_OBJECT(val)->pointer));
    }
}

template <typename Byte, typename Constant>
struct ByteCode {
    unique_ptr<Instructions<Byte>> instructions;
    vector<Constant> constants;
    using Iterator = vector<Constant>::iterator;

    size_t add(Constant c) {
        constants.push_back(c);
        return constants.size() - 1;
    }

    static int debug(Instructions<Byte> const& instructions,
                     vector<Constant> const& constants, int offset,
                     ostream& os) {
        os << setfill('0') << setw(4) << offset << " ";
        auto op = static_cast<OpCode>(instructions[offset]);
        os << SRCLANG_OPCODE_ID[static_cast<int>(op)];
        offset += 1;
        switch (op) {
            case OpCode::CONST: {
                int pos = instructions[offset++];
                if (constants.size() > 0) {
                    os << " " << pos << " '"
                       << srclang_value_print(constants[pos]) << "'";
                }

            } break;
            case OpCode::CONTINUE:
            case OpCode::BREAK:
            case OpCode::JNZ:
            case OpCode::JMP: {
                int pos = instructions[offset++];
                os << " '" << pos << "'";
            } break;
            case OpCode::LOAD:
            case OpCode::STORE: {
                int scope = instructions[offset++];
                int pos = instructions[offset++];
                os << " " << pos << " '" << SRCLANG_SYMBOL_ID[scope] << "'";
            } break;
            case OpCode::CALL: {
                int count = instructions[offset++];
                os << " '" << count << "'";
            } break;
        }

        return offset;
    }

    friend ostream& operator<<(ostream& os,
                               const ByteCode<Byte, Constant>& bytecode) {
        os << "== CODE ==" << endl;
        for (int offset = 0; offset < bytecode.instructions->size();) {
            offset = ByteCode<Byte, Constant>::debug(
                *bytecode.instructions, bytecode.constants, offset, os);
            os << endl;
        }
        os << "\n== CONSTANTS ==" << endl;
        for (auto i = 0; i < bytecode.constants.size(); i++) {
            os << i << " " << srclang_value_print(bytecode.constants[i])
               << endl;
        }
        return os;
    }
};

struct SymbolTable {
    SymbolTable* parent{nullptr};
    map<string, Symbol> store;

    Symbol define(string name) {
        store[name] =
            Symbol{name, (parent == nullptr ? Symbol::GLOBAL : Symbol::LOCAL),
                   (int)store.size()};
        return store[name];
    }

    Symbol define(string name, int index) {
        store[name] = Symbol{name, Symbol::BUILTIN, index};
        return store[name];
    }

    optional<Symbol> resolve(string name) {
        auto iter = store.find(name);
        if (iter != store.end()) {
            return iter->second;
        }
        if (parent != nullptr) {
            return parent->resolve(name);
        }
        return nullopt;
    }
};

struct MemoryManager {
    using Heap = vector<Value>;
    Heap heap;

    MemoryManager() = default;

    ~MemoryManager() {
        for (auto i = heap.begin(); i != heap.end(); i++) {
            if (SRCLANG_VALUE_IS_OBJECT(*i)) {
                auto obj = SRCLANG_VALUE_AS_OBJECT(*i);
                obj->destructor(obj->pointer);
                delete obj;
            }
        }
    }

    void mark(Heap::iterator start, Heap::iterator end) {
        for (auto i = start; i != end; i++) {
            if (SRCLANG_VALUE_IS_OBJECT(*i)) {
                SRCLANG_VALUE_AS_OBJECT(*i)->marked = true;
#ifdef SRCLANG_GC_DEBUG
                cout << "  marked "
                     << uintptr_t(SRCLANG_VALUE_AS_OBJECT(*i)->pointer) << endl;
#endif
            }
        }
    }

    void sweep() {
        for (auto i = heap.begin(); i != heap.end(); i++) {
            if (SRCLANG_VALUE_IS_OBJECT(*i)) {
                auto obj = SRCLANG_VALUE_AS_OBJECT(*i);
                if (obj->marked) {
                    obj->marked = false;
                } else {
#ifdef SRCLANG_GC_DEBUG
                    cout << "   deallocating " << uintptr_t(obj->pointer)
                         << endl;
#endif
                    obj->destructor(obj->pointer);
                    delete obj;
                    heap.erase(i);
                }
            }
        }
    }
};

enum class FunctionType { Function, Method, Initializer, Native };
template <typename Byte, typename Constant>
struct Function {
    FunctionType type;
    unique_ptr<Instructions<Byte>> instructions;
    int nlocals;
    int nparams;
    string id;
    int line;
};

template <typename Byte>
struct Interpreter;

typedef Value (*Builtin)(vector<Value>&, Interpreter<Byte>*);

template <class V>
type_info const& variant_typeid(V const& v) {
    return visit([](auto&& x) -> decltype(auto) { return typeid(x); }, v);
}

template <typename Iterator, typename Byte>
struct Compiler {
    SymbolTable* symbol_table{nullptr};
    MemoryManager* memory_manager;
    SymbolTable* global;
    Token<Iterator> cur, peek;
    Iterator iter, start, end;
    string filename;
    vector<Value>& constants;
    vector<string> loaded_imports;
    vector<unique_ptr<Instructions<Byte>>> instructions;
    using OptionType = variant<string, double, bool>;
    map<string, OptionType> options = {
        {"VERSION", double(SRCLANG_VERSION)},
        {"GC_HEAP_GROW_FACTOR", double(2)},
        {"GC_INITIAL_TRIGGER", double(30)},
        {"SEARCH_PATH", string("/usr/lib/srclang/")},
        {"LOAD_LIBC", true},
        {"LIBC", "libc.so.6"},
    };

    Compiler(Iterator start, Iterator end, string filename, SymbolTable* global,
             vector<Value>& constants, MemoryManager* memory_manager)
        : iter{start},
          start{start},
          end{end},
          constants{constants},
          global{global},
          symbol_table{global},
          filename{filename},
          memory_manager{memory_manager} {
        instructions.push_back(make_unique<Instructions<Byte>>());
        eat();
        eat();
    }

    ByteCode<Byte, Value> code() {
        return ByteCode{move(instructions.back()), constants};
    }

    Instructions<Byte>* inst() { return instructions.back().get(); }

    void push_scope() {
        symbol_table = new SymbolTable{symbol_table};
        instructions.push_back(make_unique<Instructions<Byte>>());
    }

    unique_ptr<Instructions<Byte>> pop_scope() {
        auto old = symbol_table;
        symbol_table = symbol_table->parent;
        delete old;
        auto res = move(instructions.back());
        instructions.pop_back();
        return move(res);
    }

    template <typename Message>
    void error(const Message& mesg, Iterator pos) const {
        int line;
        Iterator line_start = get_error_pos(pos, line);
        cout << filename << ":" << line << endl;
        if (pos != end) {
            cout << "ERROR: " << mesg << endl;
            cout << " | " << get_error_line(line_start) << endl << "   ";
            for (; line_start != pos; ++line_start) cout << ' ';
            cout << '^' << endl;
        } else {
            cout << "Unexpected end of file. ";
            cout << mesg << " line " << line << endl;
        }
    }

    Iterator get_error_pos(Iterator err_pos, int& line) const {
        line = 1;
        Iterator i = start;
        Iterator line_start = start;
        while (i != err_pos) {
            bool eol = false;
            if (i != err_pos && *i == '\r')  // CR
            {
                eol = true;
                line_start = ++i;
            }
            if (i != err_pos && *i == '\n')  // LF
            {
                eol = true;
                line_start = ++i;
            }
            if (eol)
                ++line;
            else
                ++i;
        }
        return line_start;
    }

    string get_error_line(Iterator err_pos) const {
        Iterator i = err_pos;
        // position i to the next EOL
        while (i != end && (*i != '\r' && *i != '\n')) ++i;
        return string(err_pos, i);
    }

    bool consume(const string& expected) {
        if (cur.type != TokenType::Reserved ||
            expected.length() != cur.literal.length() ||
            !equal(expected.begin(), expected.end(), cur.literal.begin(),
                   cur.literal.end())) {
            return false;
        }
        return eat();
    }

    bool consume(TokenType type) {
        if (cur.type != type) {
            return false;
        }
        return eat();
    }

    bool check(TokenType type) {
        if (cur.type != type) {
            stringstream ss;
            ss << "Expected '" << SRCLANG_TOKEN_ID[static_cast<int>(type)]
               << "' but got '" << cur.literal << "'";
            error(ss.str(), cur.pos);
            return false;
        }
        return true;
    }
    bool expect(const string& expected) {
        if (cur.literal != expected) {
            stringstream ss;
            ss << "Expected '" << expected << "' but got '" << cur.literal
               << "'";
            error(ss.str(), cur.pos);
            return false;
        }
        return eat();
    }

    bool expect(TokenType type) {
        if (cur.type != type) {
            stringstream ss;
            ss << "Expected '" << SRCLANG_TOKEN_ID[int(type)] << "' but got '"
               << cur.literal << "'";
            error(ss.str(), cur.pos);
            return false;
        }
        return eat();
    }

    bool eat() {
        cur = peek;

        /// space ::= [ \t\n\r]
        while (isspace(*iter)) {
            iter++;
        }
        peek.pos = iter;
        if (iter == end) {
            peek.type = TokenType::Eof;
            return true;
        }

        /// string ::= '"' ... '"'
        if (*iter == '"') {
            do {
                iter++;
                if (iter == end) {
                    error("unterminated string", peek.pos);
                    return false;
                }
            } while (*iter != '"');
            iter++;

            peek.literal = string(peek.pos + 1, iter - 1);
            peek.type = TokenType::String;

            return true;
        }

        /// reserved ::=
        for (string k : {"set", "let", "fun", "native", "return", "if", "else",
                         "for", "break", "continue", "import", "global",

                         // specical operators
                         "#!",

                         // multi char operators
                         "==", "!=", "<=", ">="}) {
            if (equal(iter, iter + distance(k.begin(), k.end()), k.begin(),
                      k.end())) {
                iter += distance(k.begin(), k.end());
                peek.literal = string(k.begin(), k.end());
                peek.type = TokenType::Reserved;
                return true;
            }
        }

        /// identifier ::= [a-zA-Z_]([a-zA-Z0-9_]*)
        if (isalpha(*iter) || *iter == '_') {
            do {
                iter++;
            } while (isalnum(*iter) || *iter == '_');
            peek.literal = string_view(peek.pos, iter);
            peek.type = TokenType::Identifier;
            return true;
        }

        /// punct ::=
        if (ispunct(*iter)) {
            peek.literal = string_view(peek.pos, ++iter);
            peek.type = TokenType::Reserved;
            return true;
        }

        /// digit ::= [0-9]
        if (isdigit(*iter)) {
            do {
                iter++;
            } while (isdigit(*iter) || *iter == '.');
            peek.literal = string_view(peek.pos, iter);
            peek.type = TokenType::Number;
            return true;
        }
        error("unexpected token", iter);
        return false;
    }

    int precedence(string tok) {
        static map<string, int> _prec = {
            {":=", 10}, {"=", 10}, {"or", 20}, {"and", 30}, {"==", 40},
            {"!=", 40}, {">", 50}, {"<", 50},  {">=", 50},  {"<=", 50},
            {"+", 60},  {"-", 60}, {"*", 70},  {"/", 70},   {"not", 80},
            {"-", 80},  {"$", 80}, {".", 90},  {"(", 90},
        };
        auto i = _prec.find(tok);
        if (i == _prec.end()) {
            return -1;
        }
        return i->second;
    }

    bool number() {
        bool is_float = false;
        for (auto i : cur.literal) {
            if (i == '.') {
                if (is_float) {
                    error("multiple floating point detected", cur.pos);
                    return false;
                }
                is_float = true;
            }
        }
        Value val;
        if (is_float) {
            val = SRCLANG_VALUE_DECIMAL(stod(cur.literal));
        } else {
            val = SRCLANG_VALUE_INTEGER(stol(cur.literal));
        }
        constants.push_back(val);
        inst()->emit(OpCode::CONST, constants.size() - 1);
        return expect(TokenType::Number);
    }

    bool identifier() {
        auto iter = find(SRCLANG_VALUE_TYPE_ID.begin(),
                         SRCLANG_VALUE_TYPE_ID.end(), cur.literal);
        if (iter != SRCLANG_VALUE_TYPE_ID.end()) {
            inst()->emit(OpCode::LOAD, Symbol::Scope::TYPE,
                         distance(SRCLANG_VALUE_TYPE_ID.begin(), iter));
            return eat();
        }
        auto symbol = symbol_table->resolve(cur.literal);
        if (symbol == nullopt) {
            error("undefined variable '" + cur.literal + "'", cur.pos);
            return false;
        }
        inst()->emit(OpCode::LOAD, symbol->scope, symbol->index);
        return eat();
    }

    bool string_() {
        auto string_value = SRCLANG_VALUE_STRING(strdup(cur.literal.c_str()));
        memory_manager->heap.push_back(string_value);
        constants.push_back(string_value);
        inst()->emit(OpCode::CONST, constants.size() - 1);
        return expect(TokenType::String);
    }

    bool unary(OpCode op) {
        if (!expression(80)) {
            return false;
        }
        inst()->emit(op);
        return true;
    }

    bool block() {
        if (!expect("{")) return false;
        while (!consume("}")) {
            if (!statement()) {
                return false;
            }
        }
        return true;
    }

    /// fun '(' args ')' block
    bool function() {
        push_scope();
        // eat '('
        if (!expect("(")) return false;
        int nparam = 0;
        while (!consume(")")) {
            if (!check(TokenType::Identifier)) {
                return false;
            }
            nparam++;
            symbol_table->define(cur.literal);
            eat();

            if (consume(")")) break;
            if (!expect(",")) return false;
        }

        if (!block()) return false;
        int nlocals = symbol_table->store.size();
        auto fun_instructions = pop_scope();

        auto fun = new Function<Byte, Value>{FunctionType::Function,
                                             move(fun_instructions),
                                             nlocals,
                                             nparam,
                                             "",
                                             0};
        auto fun_value = SRCLANG_VALUE_FUNCTION(fun);
        memory_manager->heap.push_back(fun_value);
        constants.push_back(fun_value);

        inst()->emit(OpCode::CONST, constants.size() - 1);
        inst()->emit(OpCode::FUN);
        return true;
    }

    bool prefix() {
        if (cur.type == TokenType::Number) {
            return number();
        } else if (cur.type == TokenType::String) {
            return string_();
        } else if (cur.type == TokenType::Identifier) {
            return identifier();
        } else if (consume("not")) {
            return unary(OpCode::NOT);
        } else if (consume("-")) {
            return unary(OpCode::NEG);
        } else if (consume("$")) {
            return unary(OpCode::COMMAND);
        } else if (consume("fun")) {
            return function();
        } else if (consume("(")) {
            if (!expression(-1)) {
                return false;
            }
            return consume(")");
        }

        error("Unknown expression type", cur.pos);
        return false;
    }

    bool assign() {
        error("not yet implemented", cur.pos);
        return false;
    }

    bool binary(OpCode op, int prec) {
        if (!expression(prec + 1)) {
            return false;
        }
        inst()->emit(op);
        return true;
    }

    /// call ::= '(' (expr % ',' ) ')'
    bool call() {
        int count = 0;
        while (!consume(")")) {
            count++;
            if (!expression(-1)) {
                return false;
            }
            if (consume(")")) break;
            if (!consume(",")) return false;
        }
        inst()->emit(OpCode::CALL, count);
        return true;
    }

    bool infix() {
        static map<string, OpCode> binop = {
            {"+", OpCode::ADD}, {"-", OpCode::SUB}, {"/", OpCode::DIV},
            {"*", OpCode::MUL}, {"==", OpCode::EQ}, {"!=", OpCode::NE},
            {"<", OpCode::LT},  {">", OpCode::GT},  {">=", OpCode::GE},
            {"<=", OpCode::LE},
        };

        if (consume("=")) {
            return assign();
        } else if (consume("(")) {
            return call();
        } else if (binop.find(cur.literal) != binop.end()) {
            string op = cur.literal;
            if (!eat()) return false;
            return binary(binop[op], precedence(op));
        }

        error("unexpected infix operation", cur.pos);
        return false;
    }
    bool expression(int prec) {
        if (!prefix()) {
            return false;
        }

        while ((cur.literal != ";" && cur.literal != "{") &&
               prec < precedence(cur.literal)) {
            if (!infix()) {
                return false;
            }
        }
        return true;
    }

    /// compiler_options ::= #![<option>(<value>)]
    bool compiler_options() {
        if (!expect("[")) return false;

        if (!check(TokenType::Identifier)) return false;
        auto option_id = cur.literal;
        auto pos = cur.pos;
        eat();

        auto id = options.find(option_id);
        if (id == options.end()) {
            error("unknown compiler option '" + option_id + "'", pos);
            return false;
        }
#define CHECK_TYPE_ID(ty)                                          \
    if (variant_typeid(id->second) != typeid(ty)) {                \
        error("invalid value of type '" +                          \
                  string(variant_typeid(id->second).name()) +      \
                  "' for option '" + option_id + "', required '" + \
                  string(typeid(ty).name()) + "'",                 \
              pos);                                                \
        return false;                                              \
    }
        OptionType value;
        if (consume("(")) {
            switch (cur.type) {
                case TokenType::Identifier:
                    if (cur.literal == "true" || cur.literal == "false") {
                        CHECK_TYPE_ID(bool);
                        value = cur.literal == "true";
                    } else {
                        CHECK_TYPE_ID(string);
                        value = cur.literal;
                    }
                    break;
                case TokenType::String:
                    CHECK_TYPE_ID(string);
                    value = cur.literal;
                    break;
                case TokenType::Number:
                    CHECK_TYPE_ID(double);
                    value = stod(cur.literal);
                    break;
                default:
                    CHECK_TYPE_ID(void);
            }
            eat();
            if (!expect(")")) return false;
        } else {
            CHECK_TYPE_ID(bool);
            value = true;
        }
#undef CHECK_TYPE_ID

        if (option_id == "VERSION") {
            if (SRCLANG_VERSION > get<double>(value)) {
                error(
                    "Code need srclang of version above or equal to "
                    "'" +
                        to_string(SRCLANG_VERSION) + "'",
                    pos);
                return false;
            }
        } else if (option_id == "SEARCH_PATH") {
            options[option_id] =
                get<string>(options[option_id]) + (":" + get<string>(value));
        } else {
            options[option_id] = value;
        }
        return expect("]");
    }

    /// set ::= 'set' 'global'? <identifier> '=' <expression>
    /// let ::= 'let' 'global'? <identifier> '=' <expression>
    bool variable(bool create_new) {
        bool is_global = symbol_table->parent == nullptr;
        if (consume("global")) is_global = true;

        if (!check(TokenType::Identifier)) {
            return false;
        }

        string id = cur.literal;
        auto s = is_global ? global : symbol_table;
        auto symbol = s->resolve(id);
        if (create_new) {
            if (symbol.has_value()) {
                error("Variable already defined '" + id + "'", cur.pos);
                return false;
            }
            symbol = s->define(id);
        } else {
            if (!symbol.has_value()) {
                error("Variable undefined '" + id + "'", cur.pos);
                return false;
            }
        }

        eat();

        if (!expect("=")) {
            return false;
        }

        if (!expression(-1)) {
            return false;
        }

        inst()->emit(OpCode::STORE, symbol->scope, symbol->index);
        return expect(";");
    }

    bool return_() {
        if (!expression(-1)) {
            return false;
        }
        inst()->emit(OpCode::RET);
        return expect(";");
    }

    void patch_loop(int loop_start, OpCode to_patch, int pos) {
        for (int i = loop_start; i < inst()->size();) {
            auto j = OpCode(inst()->at(i++));
            switch (j) {
                case OpCode::CONST: {
                    i++;
                } break;
                case OpCode::CONTINUE:
                case OpCode::BREAK: {
                    if (j == to_patch) inst()->at(i++) = pos;
                } break;
                case OpCode::JNZ:
                case OpCode::JMP: {
                    i++;
                } break;
                case OpCode::LOAD:
                case OpCode::STORE: {
                    i += 2;
                } break;
                case OpCode::CALL: {
                    i++;
                } break;
            }
        }
    }

    /// loop ::= 'for' <expression> <block>
    bool loop() {
        auto loop_start = inst()->size();
        if (!expression(-1)) return false;

        auto loop_exit = inst()->emit(OpCode::JNZ, 0);
        if (!block()) return false;

        patch_loop(loop_start, OpCode::CONTINUE, loop_start);

        inst()->emit(OpCode::JMP, loop_start);
        constants.push_back(SRCLANG_VALUE_NULL);
        auto after_condition =
            inst()->emit(OpCode::CONST, (int)constants.size() - 1);

        inst()->at(loop_exit + 1) = after_condition;
        patch_loop(loop_start, OpCode::BREAK, after_condition);
        return true;
    }

    /// import ::= 'import' <string>
    bool import_() {
        if (!check(TokenType::String)) return false;
        auto path = cur.literal;

        stringstream ss(get<string>(options["SEARCH_PATH"]));
        string search_path;
        bool found = false;
        while (getline(ss, search_path, ':')) {
            search_path = search_path + "/" + path + ".src";
            if (filesystem::exists(search_path)) {
                found = true;
                break;
            }
        }
        if (!found) {
            error("missing required module '" + path + "'", cur.pos);
            return false;
        }

        if (find(loaded_imports.begin(), loaded_imports.end(), search_path) !=
            loaded_imports.end()) {
            return true;
        }
        loaded_imports.push_back(search_path);

        ifstream reader(search_path);
        string input((istreambuf_iterator<char>(reader)),
                     (istreambuf_iterator<char>()));
        reader.close();

        Compiler<Iterator, Byte> compiler(input.begin(), input.end(),
                                          search_path, symbol_table, constants,
                                          memory_manager);

        if (!compiler.compile()) {
            error("failed to import '" + search_path + "'", cur.pos);
            return false;
        }
        auto instructions = move(compiler.code().instructions);
        instructions->back() = Byte(OpCode::RET);

        auto fun = new Function<Byte, Value>{
            FunctionType::Function, move(instructions), 0, 0, "", 0};
        auto val = SRCLANG_VALUE_FUNCTION(fun);
        memory_manager->heap.push_back(val);
        constants.push_back(val);
        inst()->emit(OpCode::CONST, constants.size() - 1);
        inst()->emit(OpCode::FUN);
        inst()->emit(OpCode::CALL, 0);
        eat();
        return true;
    }

    /// condition ::= 'if' <expression> <block> (else statement)?
    bool condition() {
        if (!expression(-1)) {
            return false;
        }
        auto false_pos = inst()->emit(OpCode::JNZ, 0);
        if (!block()) {
            return false;
        }

        auto jmp_pos = inst()->emit(OpCode::JMP, 0);
        auto after_block_pos = inst()->size();

        inst()->at(false_pos + 1) = after_block_pos;

        if (consume("else")) {
            if (consume("if")) {
                if (!condition()) return false;
            } else {
                if (!block()) return false;
            }
        }

        auto after_alt_pos = inst()->size();
        inst()->at(jmp_pos + 1) = after_alt_pos;

        return true;
    }
    /// native ::= 'native' <identifier> ( (<type> % ',') ) <type>
    bool native() {
        if (!check(TokenType::Identifier)) return false;
        auto id = cur.literal;
        auto symbol = symbol_table->resolve(id);
        if (symbol) {
            error("variable already defined with name '" + id + "'", cur.pos);
            return false;
        }
        symbol = symbol_table->define(id);
        vector<ValueType> types;
        ValueType ret_type;

        if (!eat()) return false;

        auto get_type = [&](string literal) -> ValueType {
            auto type = literal;
            auto iter = find(SRCLANG_VALUE_TYPE_ID.begin(),
                             SRCLANG_VALUE_TYPE_ID.end(), type);
            if (iter == SRCLANG_VALUE_TYPE_ID.end()) {
                throw runtime_error("Invalid type '" + type + "'");
            }
            return SRCLANG_VALUE_AS_TYPE(SRCLANG_VALUE_TYPES[distance(
                SRCLANG_VALUE_TYPE_ID.begin(), iter)]);
        };

        if (!expect("(")) return false;

        while (!consume(")")) {
            if (!check(TokenType::Identifier)) return false;
            try {
                types.push_back(get_type(cur.literal));
            } catch (runtime_error const& e) {
                error(e.what(), cur.pos);
                return false;
            }
            if (!eat()) return false;

            if (consume(")")) break;
            if (!expect(",")) return false;
        }
        if (!check(TokenType::Identifier)) return false;
        try {
            ret_type = get_type(cur.literal);
            if (!eat()) return false;
        } catch (runtime_error const& e) {
            error(e.what(), cur.pos);
            return false;
        }

        auto native = new NativeFunction{id, types, ret_type};
        Value val = SRCLANG_VALUE_NATIVE(native);
        memory_manager->heap.push_back(val);
        constants.push_back(val);
        inst()->emit(OpCode::CONST, constants.size() - 1);
        inst()->emit(OpCode::STORE, symbol->scope, symbol->index);
        return expect(";");
    }

    /// statement ::= set
    ///           ::= let
    ///           ::= return
    ///           ::= ';'
    ///           ::= expression ';'
    bool statement() {
        if (consume("set"))
            return variable(false);
        else if (consume("let"))
            return variable(true);
        else if (consume("return"))
            return return_();
        else if (consume(";"))
            return true;
        else if (consume("if"))
            return condition();
        else if (consume("for"))
            return loop();
        else if (consume("import"))
            return import_();
        else if (consume("break")) {
            inst()->emit(OpCode::BREAK, 0);
            return true;
        } else if (consume("continue")) {
            inst()->emit(OpCode::CONTINUE, 0);
            return true;
        } else if (consume("#!")) {
            return compiler_options();
        } else if (consume("native")) {
            return native();
        }

        if (!expression(-1)) {
            return false;
        }
        inst()->emit(OpCode::POP);
        return expect(";");
    }

    bool program() {
        while (cur.type != TokenType::Eof) {
            if (!statement()) {
                return false;
            }
        }
        return true;
    }

    bool compile() {
        if (!program()) {
            return false;
        }
        inst()->emit(OpCode::HLT);
        return true;
    }
};

#define SRCLANG_BUILTIN_LIST \
    X(println)               \
    X(gc)
template <typename Byte>
struct Interpreter;
#define X(id) SRCLANG_BUILTIN(id);
SRCLANG_BUILTIN_LIST
#undef X

const vector<Value> builtins = {
#define X(id) SRCLANG_VALUE_BUILTIN(id),
    SRCLANG_BUILTIN_LIST
#undef X
};

template <typename Byte>
struct Interpreter {
    struct Frame {
        vector<Byte>::iterator ip;
        Function<Byte, Value>* fun;
        vector<Value>::iterator bp;
    };

    int next_gc = 20;
    int GC_HEAP_GROW_FACTOR = 2;

    vector<Value> stack;
    vector<Value>::iterator sp;
    MemoryManager* memory_manager;

    vector<Value>& globals;
    vector<Value>& constants;
    vector<Frame> frames;
    vector<Frame>::iterator fp;

    bool debug;

    Interpreter(ByteCode<Byte, Value>& code, vector<Value>& globals,
                MemoryManager* memory_manager)
        : stack(1024),
          frames(256),
          globals{globals},
          constants{code.constants},
          memory_manager{memory_manager} {
        sp = stack.begin();
        fp = frames.begin();
        auto fun = new Function<Byte, Value>{
            FunctionType::Function, move(code.instructions), 0, 0, "", 0};
        fp->fun = move(fun);
        fp->ip = fp->fun->instructions->begin();
        fp->bp = sp;
        fp++;
    }

    ~Interpreter() { delete (frames.begin())->fun; }

    void add_object(Value val) {
#ifdef SRCLANG_GC_DEBUG
        gc();
#else
        if (memory_manager->heap.size() > next_gc) {
            gc();
            next_gc = memory_manager->heap.size() * GC_HEAP_GROW_FACTOR;
        }
#endif
        memory_manager->heap.push_back(val);
    }

    vector<Byte>::iterator& ip() { return (fp - 1)->ip; }

    vector<Frame>::iterator cur() { return (fp - 1); }

    void gc() {
#ifdef SRCLANG_GC_DEBUG
        cout << "Total allocations: " << memory_manager->heap.size() << endl;
        cout << "gc begin:" << endl;
#endif
        memory_manager->mark(stack.begin(), sp);
        memory_manager->mark(globals.begin(), globals.end());
        memory_manager->mark(constants.begin(), constants.end());
        memory_manager->sweep();
#ifdef SRCLANG_GC_DEBUG
        cout << "gc end:" << endl;
        cout << "Total allocations: " << memory_manager->heap.size() << endl;
#endif
    }

    bool unary(Value a, OpCode op) {
        if (srclang_value_get_type(a) == ValueType::Integer) {
            switch (op) {
                case OpCode::NOT:
                    *sp++ = SRCLANG_VALUE_INTEGER(!SRCLANG_VALUE_AS_INTEGER(a));
                    break;
                case OpCode::NEG:
                    *sp++ = SRCLANG_VALUE_INTEGER(-SRCLANG_VALUE_AS_INTEGER(a));
                    break;
                default:
                    cerr
                        << "ERROR: unexpected unary operator '"
                        << SRCLANG_OPCODE_ID[static_cast<int>(op)] << "' for '"
                        << SRCLANG_VALUE_TYPE_ID[int(srclang_value_get_type(a))]
                        << "'" << endl;
                    return false;
            }
        } else if (srclang_value_get_type(a) == ValueType::Boolean) {
            switch (op) {
                case OpCode::NOT:
                    *sp++ = !a;
                    break;
                default:
                    cerr
                        << "ERROR: unexpected unary operator '"
                        << SRCLANG_OPCODE_ID[static_cast<int>(op)] << "' for '"
                        << SRCLANG_VALUE_TYPE_ID[int(srclang_value_get_type(a))]
                        << "'" << endl;
                    return false;
            }
        } else if (srclang_value_get_type(a) == ValueType::String) {
            switch (op) {
                case OpCode::COMMAND: {
                    array<char, 128> buffer;
                    string result;
                    unique_ptr<FILE, decltype(&pclose)> pipe(
                        popen((char*)(SRCLANG_VALUE_AS_OBJECT(a))->pointer,
                              "r"),
                        pclose);
                    if (!pipe) {
                        throw runtime_error("popen() failed!");
                    }
                    while (fgets(buffer.data(), buffer.size(), pipe.get()) !=
                           nullptr) {
                        result += buffer.data();
                    }
                    auto string_value =
                        SRCLANG_VALUE_STRING(strdup(result.c_str()));
                    add_object(string_value);
                    *sp++ = string_value;
                } break;
                default:
                    cerr
                        << "ERROR: unexpected unary operator '"
                        << SRCLANG_OPCODE_ID[static_cast<int>(op)] << "' for '"
                        << SRCLANG_VALUE_TYPE_ID[int(srclang_value_get_type(a))]
                        << "'" << endl;
                    return false;
            }
        } else {
            cerr << "ERROR: unhandler unary operation for value of type "
                 << SRCLANG_VALUE_TYPE_ID[static_cast<int>(
                        srclang_value_get_type(a))]
                 << "'" << endl;
            return false;
        }

        return true;
    }

    bool binary(Value lhs, Value rhs, OpCode op) {
        if (srclang_value_get_type(lhs) == ValueType::Integer) {
            auto a = SRCLANG_VALUE_AS_INTEGER(lhs);
            auto b = SRCLANG_VALUE_AS_INTEGER(rhs);
            switch (op) {
                case OpCode::ADD:
                    *sp++ = SRCLANG_VALUE_INTEGER(a + b);
                    break;
                case OpCode::SUB:
                    *sp++ = SRCLANG_VALUE_INTEGER(a - b);
                    break;
                case OpCode::MUL:
                    *sp++ = SRCLANG_VALUE_INTEGER(a * b);
                    break;
                case OpCode::DIV:
                    *sp++ = SRCLANG_VALUE_INTEGER(a / b);
                    break;
                case OpCode::EQ:
                    *sp++ = a == b ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE;
                    break;
                case OpCode::NE:
                    *sp++ = a != b ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE;
                    break;
                case OpCode::LT:
                    *sp++ = a < b ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE;
                    break;
                case OpCode::LE:
                    *sp++ = a <= b ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE;
                    break;
                case OpCode::GT:
                    *sp++ = a > b ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE;
                    break;
                case OpCode::GE:
                    *sp++ = a >= b ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE;
                    break;
                default:
                    cerr << "ERROR: unexpected binary operator '"
                         << SRCLANG_OPCODE_ID[static_cast<int>(op)] << "'"
                         << endl;
                    return false;
            }
        } else if (srclang_value_get_type(lhs) == ValueType::String) {
            char* a =
                reinterpret_cast<char*>(SRCLANG_VALUE_AS_OBJECT(lhs)->pointer);
            char* b =
                reinterpret_cast<char*>(SRCLANG_VALUE_AS_OBJECT(rhs)->pointer);
            switch (op) {
                case OpCode::ADD: {
                    int size = strlen(a) + strlen(b) + 1;
                    char* buf = new char[size];
                    snprintf(buf, size, "%s%s", a, b);
                    buf[size] = '\0';
                    auto val = SRCLANG_VALUE_STRING(buf);
                    add_object(val);
                    *sp++ = val;
                } break;
                default:
                    cerr << "ERROR: unexpected binary operator '"
                         << SRCLANG_OPCODE_ID[static_cast<int>(op)] << "'"
                         << endl;
                    return false;
            }
        } else {
            cerr << "ERROR: unsupported binary operator for type '"
                 << SRCLANG_VALUE_TYPE_ID[static_cast<int>(
                        srclang_value_get_type(lhs))]
                 << "'" << endl;
            return false;
        }

        return true;
    }

    bool is_falsy(Value val) {
        return SRCLANG_VALUE_IS_NULL(val) ||
               (SRCLANG_VALUE_IS_BOOL(val) &&
                SRCLANG_VALUE_AS_BOOL(val) == false) ||
               (SRCLANG_VALUE_IS_INTEGER(val) &&
                SRCLANG_VALUE_AS_INTEGER(val) == 0);
    }

    bool call_function(Value callee, uint8_t count) {
        auto fun = reinterpret_cast<Function<Byte, Value>*>(
            SRCLANG_VALUE_AS_OBJECT(callee)->pointer);
        if (count != fun->nparams) {
            cerr << "Invalid parameters" << endl;
            return false;
        }
        fp->fun = move(fun);
        fp->ip = fp->fun->instructions->begin();
        fp->bp = (sp - count);
        sp = fp->bp + fun->nlocals;
        fp++;
        return true;
    }

    bool call_builtin(Value callee, uint8_t count) {
        auto builtin =
            reinterpret_cast<Builtin>(SRCLANG_VALUE_AS_OBJECT(callee)->pointer);
        vector<Value> args(sp - count, sp);
        sp -= count + 1;
        try {
            *sp++ = builtin(args, this);
        } catch (runtime_error const& e) {
            cerr << "ERROR: " << e.what() << endl;
            return false;
        }

        return true;
    }

    bool call_typecast_num(uint8_t count) {
        if (count != 1 || !SRCLANG_VALUE_IS_OBJECT(*(sp - count)) ||
            SRCLANG_VALUE_AS_OBJECT(*(sp - count))->type != ValueType::String) {
            cerr << "ERROR: invalid typecast to num()" << endl;
            return false;
        }
        Value val = *(sp - count);
        sp -= count + 1;
        try {
            *sp++ = stod((char*)(SRCLANG_VALUE_AS_OBJECT(val)->pointer));
        } catch (...) {
            cerr << "ERROR: invalid typecast to num()" << endl;
            return false;
        }
        return true;
    }

    bool call_typecast_string(uint8_t count) {
        string buf;
        for (auto i = sp - count; i != sp; i++) {
            buf += srclang_value_print(*i);
        }
        sp -= count + 1;
        *sp++ = SRCLANG_VALUE_STRING(strdup(buf.c_str()));
        return true;
    }

    bool call_typecast_bool(uint8_t count) {
        if (count != 1) {
            cerr << "ERROR: invalid typecast to bool()" << endl;
            return false;
        }
        Value val = *(sp - count);
        sp -= count + 1;
        *sp++ = SRCLANG_VALUE_BOOL(!is_falsy(val));
        return true;
    }

    bool call_typecast_type(uint8_t count) {
        if (count != 1) {
            cerr << "ERROR: invalid typecast to type()" << endl;
            return false;
        }
        Value val = *(sp - count);
        sp -= count + 1;
        *sp++ = SRCLANG_VALUE_TYPE(srclang_value_get_type(val));
        return true;
    }

    bool call_typecast(Value callee, uint8_t count) {
        auto type = SRCLANG_VALUE_AS_TYPE(callee);
        switch (type) {
            case ValueType::Integer:
                return call_typecast_num(count);
            case ValueType::Type:
                return call_typecast_type(count);
            case ValueType::Boolean:
                return call_typecast_bool(count);
            case ValueType::String:
                return call_typecast_string(count);
            // case ValueType::Function:
            //     return call_typecast_function(count);
            default:
                cerr << "ERROR: invalid typecast '"
                     << SRCLANG_VALUE_TYPE_ID[int(type)] << "'" << endl;
                return false;
        }
    }

    bool call_native(Value callee, uint8_t count) {
        auto native = reinterpret_cast<NativeFunction*>(
            SRCLANG_VALUE_AS_OBJECT(callee)->pointer);
        if (native->param.size() != count) {
            cerr << "ERROR: invalid parameter" << endl;
            return false;
        }
        void* handler = dlsym(nullptr, native->id.c_str());
        if (handler == nullptr) {
            cerr << dlerror() << endl;
            return false;
        }

        ffi_cif cif;
        void* values[count];
        ffi_type* types[count];
        int j = 0;
        for (auto i = sp - count; i != sp; i++, j++) {
            auto type = srclang_value_get_type(*i);
            if (type != native->param[j]) {
                cerr << "ERROR: invalid " << j << "th parameter" << endl;
                return false;
            }
            switch (type) {
                case ValueType::Null:
                    values[j] = nullptr;
                    types[j] = &ffi_type_pointer;
                    break;
                case ValueType::Integer: {
                    values[j] = &(*i);
                    (*(int64_t*)(values[j])) >>= 3;
                    types[j] = &ffi_type_slong;

                } break;
                case ValueType::Decimal: {
                    values[j] = &(*i);
                    types[j] = &ffi_type_double;
                } break;
                case ValueType::Boolean:
                    values[j] = &(*i);
                    types[j] = &ffi_type_sint;
                    break;
                default:
                    values[j] = &SRCLANG_VALUE_AS_OBJECT(*i)->pointer;
                    types[j] = &ffi_type_pointer;
                    break;
            }
        }
        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, count, &ffi_type_slong,
                         types) != FFI_OK) {
            cerr << "ffi_prep_cif() failed" << endl;
            return false;
        }
        ffi_arg result;
        Value result_value;
        ffi_call(&cif, FFI_FN(handler), &result, values);
        cout << "RESULT: " << result << endl;
        switch (native->ret) {
            case ValueType::Boolean:
                result_value = SRCLANG_VALUE_BOOL(result != 0);
                break;
            case ValueType::Integer:
                result_value = SRCLANG_VALUE_INTEGER(result);
                break;
            case ValueType::Decimal:
                result_value = SRCLANG_VALUE_DECIMAL(result);
                break;
            case ValueType::String:
                result_value = SRCLANG_VALUE_STRING((char*)result);
                break;
            default:
                cerr << "ERROR: unsupported return type '"
                     << SRCLANG_VALUE_TYPE_ID[int(native->ret)] << "'" << endl;
                return false;
        }

        sp -= count + 1;
        *sp++ = result_value;
        return true;
    }

    bool call(uint8_t count) {
        auto callee = *(sp - 1 - count);
        switch (srclang_value_get_type(callee)) {
            case ValueType::Function:
                return call_function(callee, count);
            case ValueType::Builtin:
                return call_builtin(callee, count);
            case ValueType::Type:
                return call_typecast(callee, count);
            case ValueType::Native:
                return call_native(callee, count);

            default:
                cerr << "ERROR: can't call value of type '"
                     << SRCLANG_VALUE_TYPE_ID[static_cast<int>(
                            srclang_value_get_type(callee))]
                     << "'" << endl;
                return false;
        }
    }

    bool run() {
        while (true) {
            if (debug) {
                cout << "  ";
                for (auto i = stack.begin(); i != sp; i++) {
                    cout << "[" << srclang_value_print(*i) << "] ";
                }
                cout << endl;
                ByteCode<Byte, Value>::debug(
                    *cur()->fun->instructions.get(), constants,
                    distance(cur()->fun->instructions->begin(), ip()), cout);
                cout << endl;
            }
            auto inst = static_cast<OpCode>(*ip()++);
            switch (inst) {
                case OpCode::CONST:
                    *sp++ = constants[*ip()++];
                    break;

                case OpCode::ADD:
                case OpCode::SUB:
                case OpCode::MUL:
                case OpCode::DIV:
                case OpCode::EQ:
                case OpCode::NE:
                case OpCode::LT:
                case OpCode::LE:
                case OpCode::GT:
                case OpCode::GE: {
                    auto b = *--sp;
                    auto a = *--sp;
                    if (!binary(a, b, inst)) {
                        return false;
                    }
                } break;

                case OpCode::COMMAND:
                case OpCode::NOT:
                case OpCode::NEG: {
                    if (!unary(*--sp, inst)) {
                        return false;
                    }
                } break;

                case OpCode::STORE: {
                    auto scope = Symbol::Scope(*ip()++);
                    int pos = *ip()++;

                    switch (scope) {
                        case Symbol::Scope::LOCAL:
                            *(cur()->bp + pos) = *--sp;
                            break;
                        case Symbol::Scope::GLOBAL:
                            globals[pos] = *--sp;
                            break;
                    }
                } break;

                case OpCode::LOAD: {
                    auto scope = Symbol::Scope(*ip()++);
                    int pos = *ip()++;
                    switch (scope) {
                        case Symbol::Scope::LOCAL:
                            *sp++ = *(cur()->bp + pos);
                            break;
                        case Symbol::Scope::GLOBAL:
                            *sp++ = globals[pos];
                            break;
                        case Symbol::Scope::BUILTIN:
                            *sp++ = builtins[pos];
                            break;
                        case Symbol::Scope::TYPE:
                            *sp++ = SRCLANG_VALUE_TYPES[pos];
                            break;
                        default:
                            cerr << "ERROR: can't load value of scope '"
                                 << SRCLANG_SYMBOL_ID[int(scope)] << "'"
                                 << endl;
                            return false;
                    }

                } break;

                case OpCode::FUN: {
                } break;

                case OpCode::CALL: {
                    int count = *ip()++;
                    if (!call(count)) {
                        return false;
                    }
                } break;

                case OpCode::POP: {
                    *--sp;
                } break;

                case OpCode::RET: {
                    auto value = *--sp;
                    sp = cur()->bp - 1;
                    fp--;
                    *sp++ = value;
                } break;

                case OpCode::JNZ: {
                    auto value = *--sp;
                    if (!SRCLANG_VALUE_AS_BOOL(value)) {
                        ip() = (cur()->fun->instructions->begin() + *ip());
                    } else {
                        *ip()++;
                    }
                } break;

                case OpCode::CONTINUE:
                case OpCode::BREAK:
                case OpCode::JMP: {
                    ip() = (cur()->fun->instructions->begin() + *ip());
                } break;

                case OpCode::HLT: {
                    return true;
                } break;

                default:
                    cerr << "ERROR: unknown opcode '"
                         << SRCLANG_OPCODE_ID[static_cast<int>(inst)] << "'"
                         << endl;
                    return false;
            }
        }
    }
};

SRCLANG_BUILTIN(gc) {
    interpreter->gc();
    return SRCLANG_VALUE_TRUE;
}

SRCLANG_BUILTIN(println) {
    for (auto const& i : args) {
        cout << srclang_value_print(i);
    }
    cout << endl;
    return SRCLANG_VALUE_INTEGER(args.size());
}

int main(int argc, char** argv) {
    SymbolTable symbol_table;

    bool is_interactive = true;
    optional<string> filename = nullopt;
    MemoryManager memory_manager;
    for (auto const& i : builtins) memory_manager.heap.push_back(i);

    bool debug = false;
    for (int i = 1; i < argc; i++) {
        string arg(argv[i]);
        if (arg[0] == '-') {
            arg = arg.substr(1);
            if (arg == "debug")
                debug = true;
            else {
                cerr << "ERROR: unknown flag '-" << arg << "'" << endl;
                return 1;
            }
        } else if (filename == nullopt && filesystem::exists(arg) &&
                   filesystem::path(arg).has_extension() &&
                   filesystem::path(arg).extension() == ".src") {
            filename = arg;
        } else {
            cerr << "ERROR: unknown argument " << arg << endl;
            return 1;
        }
    }

    string input;
    if (filename.has_value()) {
        ifstream reader(*filename);
        input = string((istreambuf_iterator<char>(reader)),
                       (istreambuf_iterator<char>()));
        is_interactive = false;
    }
    vector<Value> globals(20);
    {
        int i = 0;
#define X(id) symbol_table.define(#id, i++);
        SRCLANG_BUILTIN_LIST
#undef X
    }

    auto true_symbol = symbol_table.define("true");
    auto false_symbol = symbol_table.define("false");
    auto null_symbol = symbol_table.define("null");
    globals[true_symbol.index] = SRCLANG_VALUE_TRUE;
    globals[false_symbol.index] = SRCLANG_VALUE_FALSE;
    globals[null_symbol.index] = SRCLANG_VALUE_NULL;

    do {
        if (is_interactive) {
            cout << ">> ";
            if (!getline(cin, input)) {
                continue;
            }
        }
        vector<Value> constants;

        Compiler<Iterator, Byte> compiler(
            input.begin(), input.end(),
            (filename == nullopt ? "<script>" : *filename), &symbol_table,
            constants, &memory_manager);
        if (!compiler.compile()) {
            continue;
        }

        if (get<bool>(compiler.options["LOAD_LIBC"])) {
            if (dlopen(get<string>(compiler.options["LIBC"]).c_str(),
                       RTLD_GLOBAL | RTLD_NOW) == nullptr) {
                cerr << "FAILED to load libc '" << dlerror() << "'" << endl;
                return 1;
            }
        }

        auto code = move(compiler.code());
        if (debug) {
            cout << code;
            cout << "== GLOBALS ==" << endl;
            for (auto const& i : symbol_table.store) {
                if (i.second.scope == Symbol::GLOBAL) {
                    cout << "- " << i.first << " := "
                         << srclang_value_print(globals[i.second.index])
                         << endl;
                }
            }
        }

        Interpreter<Byte> interpreter{code, globals, &memory_manager};
        interpreter.GC_HEAP_GROW_FACTOR =
            std::get<double>(compiler.options["GC_HEAP_GROW_FACTOR"]);
        interpreter.next_gc =
            std::get<double>(compiler.options["GC_INITIAL_TRIGGER"]);
        interpreter.debug = debug;
        if (!interpreter.run()) {
            continue;
        }
    } while (is_interactive);
}