#ifndef SRCLANG_VALUE_HXX
#define SRCLANG_VALUE_HXX

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace srclang {
#define SRCLANG_VERSION 20230221
    static const char *MAGIC_CODE = "SRCLANG";
#define SRCLANG_VALUE_TYPE_LIST \
    X(Null, "null_t")           \
    X(Boolean, "bool")          \
    X(Number, "num")            \
    X(String, "str")            \
    X(List, "list")             \
    X(Map, "map")               \
    X(Function, "function")     \
    X(Closure, "closure")       \
    X(Builtin, "builtin")       \
    X(Error, "error")           \
    X(Bounded, "bounded")       \
    X(Type, "type")             \
    X(Pointer, "ptr")

    enum class ValueType : uint8_t {
#define X(id, name) id,
        SRCLANG_VALUE_TYPE_LIST
#undef X
    };

    static const std::vector<std::string> SRCLANG_VALUE_TYPE_ID = {
#define X(id, name) name,
        SRCLANG_VALUE_TYPE_LIST
#undef X
    };

    struct HeapObject;

    typedef uint64_t Value;

#define SRCLANG_VALUE_SIGN_BIT ((uint64_t)0x8000000000000000)
#define SRCLANG_VALUE_QNAN ((uint64_t)0x7ffc000000000000)

#define SRCLANG_VALUE_TAG_NULL 1
#define SRCLANG_VALUE_TAG_FALSE 2
#define SRCLANG_VALUE_TAG_TRUE 3
#define SRCLANG_VALUE_TAG_TYPE 4
#define SRCLANG_VALUE_TAG_1 5
#define SRCLANG_VALUE_TAG_2 6
#define SRCLANG_VALUE_TAG_3 7

#define SRCLANG_VALUE_IS_BOOL(val) (((val) | 1) == SRCLANG_VALUE_TRUE)
#define SRCLANG_VALUE_IS_NULL(val) ((val) == SRCLANG_VALUE_NULL)
#define SRCLANG_VALUE_IS_NUMBER(val) \
    (((val) & SRCLANG_VALUE_QNAN) != SRCLANG_VALUE_QNAN)
#define SRCLANG_VALUE_IS_OBJECT(val)                            \
    (((val) & (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_SIGN_BIT)) == \
     (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_SIGN_BIT))

#define SRCLANG_VALUE_IS_TYPE(val)                              \
    (((val) & (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_TYPE)) == \
         (SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_TYPE) &&       \
     ((val | SRCLANG_VALUE_SIGN_BIT) != val))

#define SRCLANG_VALUE_AS_BOOL(val) ((val) == SRCLANG_VALUE_TRUE)
#define SRCLANG_VALUE_AS_NUMBER(val) (srclang_value_to_decimal(val))
#define SRCLANG_VALUE_AS_OBJECT(val)   \
    ((HeapObject *)(uintptr_t)((val) & \
                               ~(SRCLANG_VALUE_SIGN_BIT | SRCLANG_VALUE_QNAN)))
#define SRCLANG_VALUE_AS_TYPE(val) ((ValueType)((val) >> 3))

#define SRCLANG_VALUE_BOOL(b) ((b) ? SRCLANG_VALUE_TRUE : SRCLANG_VALUE_FALSE)
#define SRCLANG_VALUE_NUMBER(num) (srclang_decimal_to_value((double)(num)))
#define SRCLANG_VALUE_OBJECT(obj)                         \
    (Value)(SRCLANG_VALUE_SIGN_BIT | SRCLANG_VALUE_QNAN | \
            (uint64_t)(uintptr_t)(obj))
#define SRCLANG_VALUE_TYPE(ty)                                      \
    ((Value)(SRCLANG_VALUE_QNAN | ((uint64_t)(uint32_t)(ty) << 3) | \
             SRCLANG_VALUE_TAG_TYPE))

#define SRCLANG_VALUE_HEAP_OBJECT(type, ptr) \
    SRCLANG_VALUE_OBJECT(                    \
        (new HeapObject{(type), (ptr)}))

#define SRCLANG_VALUE_STRING(str) \
    SRCLANG_VALUE_HEAP_OBJECT(    \
        ValueType::String, (void *)str)

#define SRCLANG_VALUE_LIST(list) \
    SRCLANG_VALUE_HEAP_OBJECT(ValueType::List, (void *)list)

#define SRCLANG_VALUE_MAP(map) \
    SRCLANG_VALUE_HEAP_OBJECT( \
        ValueType::Map, (void *)map)

#define SRCLANG_VALUE_NATIVE(native) \
    SRCLANG_VALUE_HEAP_OBJECT(ValueType::Native, (void *)native)

#define SRCLANG_VALUE_ERROR(err) \
    SRCLANG_VALUE_HEAP_OBJECT(   \
        ValueType::Error, (void *)err)

#define SRCLANG_VALUE_BUILTIN(id) \
    SRCLANG_VALUE_HEAP_OBJECT(    \
        ValueType::Builtin, (void *)srclang::builtin_##id)

#define SRCLANG_VALUE_BUILTIN_NEW(v) \
    SRCLANG_VALUE_HEAP_OBJECT(       \
        ValueType::Builtin, (void *)(v))

#define SRCLANG_VALUE_FUNCTION(fun) \
    SRCLANG_VALUE_HEAP_OBJECT(      \
        ValueType::Function, (void *)fun)

#define SRCLANG_VALUE_CLOSURE(fun) \
    SRCLANG_VALUE_HEAP_OBJECT(     \
        ValueType::Closure, (void *)fun)

#define SRCLANG_VALUE_BOUNDED(b) \
    SRCLANG_VALUE_HEAP_OBJECT(   \
        ValueType::Bounded, (void *)(b))

#define SRCLANG_VALUE_BOUND(p, c) \
    SRCLANG_VALUE_BOUNDED((new BoundedValue{(p), (c)}))

#define SRCLANG_VALUE_POINTER(ptr) SRCLANG_VALUE_HEAP_OBJECT(ValueType::Pointer, ptr)

#define SRCLANG_VALUE_SET_REF(val) srclang_value_set_ref(val)
#define SRCLANG_VALUE_SET_SIZE(val, sz) srclang_value_set_size(val, sz)
#define SRCLANG_VALUE_SET_CLEANUP(val, cl) srclang_value_set_cleanup(val, cl)
#define SRCLANG_VALUE_GET_SIZE(val) srclang_value_get_size(val)

#define SRCLANG_VALUE_TRUE \
    ((Value)(uint64_t)(SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_TRUE))
#define SRCLANG_VALUE_FALSE \
    ((Value)(uint64_t)(SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_FALSE))
#define SRCLANG_VALUE_NULL \
    ((Value)(uint64_t)(SRCLANG_VALUE_QNAN | SRCLANG_VALUE_TAG_NULL))

#define SRCLANG_VALUE_DEBUG(val) SRCLANG_VALUE_GET_STRING(val) + ":" + SRCLANG_VALUE_TYPE_ID[(int)SRCLANG_VALUE_GET_TYPE(val)]

#define SRCLANG_MODULE_VALUE(id) __srclang__module__value__##id
#define SRCLANG_MODULE_OBJECT(id) __srclang__module__object__##id

#define SRCLANG_MODULE(id) \
    extern "C" Value __srclang_module__()

#define SRCLANG_MODULE_FUNCTION(id) __srclang_module_fun_##id

#define SRCLANG_MODULE_FUNCTION_DEFINE(id) \
    static Value SRCLANG_MODULE_FUNCTION(id)(std::vector<Value> & args, Interpreter * interpreter)

#define SRCLANG_MODULE_OBJECT_CREATE(id) auto SRCLANG_MODULE_OBJECT(id) = new SrcLangMap()
#define SRCLANG_MODULE_VALUE_CREATE(id) auto SRCLANG_MODULE_VALUE(id) = SRCLANG_VALUE_MAP(SRCLANG_MODULE_OBJECT(id))

#define SRCLANG_MODULE_FUNCTION_ADD(modid, funid) SRCLANG_MODULE_OBJECT(modid)->insert({#funid, SRCLANG_VALUE_BUILTIN_NEW(SRCLANG_MODULE_FUNCTION(funid))})

#define SRCLANG_MODULE_PROLOGUE(id)   \
    SRCLANG_MODULE_OBJECT_CREATE(id); \
    SRCLANG_MODULE_VALUE_CREATE(id);

#define SRCLANG_MODULE_EPILOGUE(id) \
    return SRCLANG_MODULE_VALUE(id);

#define SRCLANG_CHECK_ARGS_EXACT(count)                                    \
    if (args.size() != count)                                              \
        throw std::runtime_error("Expected '" + std::to_string(count) +    \
                                 "' but '" + std::to_string(args.size()) + \
                                 "' provided");

#define SRCLANG_CHECK_ARGS_ATLEAST(count)                                       \
    if (args.size() < count)                                                    \
        throw std::runtime_error("Expected atleast '" + std::to_string(count) + \
                                 "' but '" + std::to_string(args.size()) +      \
                                 "' provided");

#define SRCLANG_CHECK_ARGS_RANGE(start, end)                               \
    if (args.size() < start || args.size() > end)                          \
        throw std::runtime_error("Expected '" + std::to_string(start) +    \
                                 "':'" + std::to_string(end) + "' but '" + \
                                 std::to_string(args.size()) + "' provided");

#define SRCLANG_CHECK_ARGS_TYPE(pos, ty)                              \
    if (SRCLANG_VALUE_GET_TYPE(args[pos]) != ty)                      \
        throw std::runtime_error("Expected '" + std::to_string(pos) + \
                                 "' to be '" +                        \
                                 SRCLANG_VALUE_TYPE_ID[int(ty)] + "'");

    typedef std::vector<Value> SrcLangList;
    typedef std::map<std::string, Value> SrcLangMap;

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

    struct BoundedValue {
        Value parent;
        Value value;
    };

    static const std::vector<Value> SRCLANG_VALUE_TYPES = {
#define X(id, name) SRCLANG_VALUE_TYPE(ValueType::id),
        SRCLANG_VALUE_TYPE_LIST
#undef X
    };

    ValueType SRCLANG_VALUE_GET_TYPE(Value val);

    std::string SRCLANG_VALUE_GET_STRING(Value val);

    void SRCLANG_VALUE_FREE(Value value);
}

#endif
