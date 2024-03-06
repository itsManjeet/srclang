#include "Builtin.h"

#include "../Interpreter/Interpreter.h"
#include "../Language/Language.h"

#include <ffi.h>

#ifdef _WIN32

#include <windows.h>

#else
#include <dlfcn.h>
#endif

using namespace srclang;


std::vector<Value> srclang::builtins = {
#define X(id) SRCLANG_VALUE_BUILTIN(id),
        SRCLANG_BUILTIN_LIST
#undef X
};

#undef SRCLANG_BUILTIN

#define SRCLANG_BUILTIN(id)                                     \
    Value srclang::builtin_##id(std::vector<Value> const &args, \
                                Interpreter *interpreter)

SRCLANG_BUILTIN(gc) {
    SRCLANG_CHECK_ARGS_EXACT(0);
    interpreter->gc();

    return SRCLANG_VALUE_TRUE;
}

SRCLANG_BUILTIN(call) {
    SRCLANG_CHECK_ARGS_ATLEAST(1);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Closure);
    auto callee = args[0];
    SrcLangList callee_args;
    if (args.size() > 1) {
        callee_args = std::vector<Value>(args.begin() + 1, args.end());
    }
    return interpreter->language->call(callee, callee_args);
}

SRCLANG_BUILTIN(print) {
    for (auto const &i: args) {
        std::cout << SRCLANG_VALUE_GET_STRING(i);
    }
    return SRCLANG_VALUE_NUMBER(args.size());
}

SRCLANG_BUILTIN(println) {
    std::string sep;
    for (auto const &i: args) {
        std::cout << sep << SRCLANG_VALUE_GET_STRING(i);
        sep = " ";
    }
    std::cout << std::endl;
    return SRCLANG_VALUE_NUMBER(args.size());
}

SRCLANG_BUILTIN(len) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    switch (SRCLANG_VALUE_GET_TYPE(args[0])) {
        case ValueType::String:
            return SRCLANG_VALUE_NUMBER(strlen((char *) SRCLANG_VALUE_AS_OBJECT(args[0])->pointer));
        case ValueType::List:
            return SRCLANG_VALUE_NUMBER(((std::vector<Value> *) SRCLANG_VALUE_AS_OBJECT(args[0])->pointer)->size());
        case ValueType::Pointer:
            return SRCLANG_VALUE_NUMBER(SRCLANG_VALUE_AS_OBJECT(args[0])->size);

        default:
            return SRCLANG_VALUE_NUMBER(sizeof(Value));
    }
}

SRCLANG_BUILTIN(append) {
    SRCLANG_CHECK_ARGS_EXACT(2);
    switch (SRCLANG_VALUE_GET_TYPE(args[0])) {
        case ValueType::List: {
            auto list = reinterpret_cast<SrcLangList *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
            list->push_back(args[1]);
            return SRCLANG_VALUE_LIST(list);
        }
            break;
        case ValueType::String: {
            auto str = (char *) (SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
            auto len = strlen(str);
            switch (SRCLANG_VALUE_GET_TYPE(args[1])) {
                break;
                case ValueType::String: {
                    auto str2 = (char *) (SRCLANG_VALUE_AS_OBJECT(args[1])->pointer);
                    auto len2 = strlen(str2);
                    str = (char *) realloc(str, len + len2 + 1);
                    if (str == nullptr) {
                        throw std::runtime_error(
                                "realloc() failed, srclang::append(), " + std::string(strerror(errno)));
                    }
                    strcat(str, str2);
                }
                    break;
                default:
                    throw std::runtime_error("invalid append operation");
            }
            return SRCLANG_VALUE_STRING(str);
        }
            break;
        default:
            return SRCLANG_VALUE_ERROR(strdup(("invalid append operation on '" +
                                               SRCLANG_VALUE_TYPE_ID[int(SRCLANG_VALUE_GET_TYPE(args[0]))] +
                                               "'").c_str()));
    }
}

SRCLANG_BUILTIN(range) {
    SRCLANG_CHECK_ARGS_RANGE(1, 3);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Number);
    int start = 0;
    int inc = 1;
    int end = SRCLANG_VALUE_AS_NUMBER(args[0]);

    if (args.size() == 2) {
        start = end;
        end = SRCLANG_VALUE_AS_NUMBER(args[1]);
    } else if (args.size() == 3) {
        start = end;
        end = SRCLANG_VALUE_AS_NUMBER(args[1]);
        inc = SRCLANG_VALUE_AS_NUMBER(args[2]);
    }

    if (inc < 0) {
        auto t = start;
        start = end;
        end = start;
        inc = -inc;
    }

    auto list = new SrcLangList();

    for (int i = start; i < end; i += inc) {
        list->push_back(SRCLANG_VALUE_NUMBER(i));
    }
    return SRCLANG_VALUE_LIST(list);
}

SRCLANG_BUILTIN(pop) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    switch (SRCLANG_VALUE_GET_TYPE(args[0])) {
        case ValueType::List: {
            auto list = reinterpret_cast<SrcLangList *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
            list->pop_back();

            return SRCLANG_VALUE_LIST(list);
        }
            break;
        case ValueType::String: {
            auto str = (char *) (SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
            auto len = strlen(str);
            str[len - 1] = '\0';
            return SRCLANG_VALUE_STRING(str);
        }
            break;
        default:
            return SRCLANG_VALUE_ERROR(
                    strdup(("invalid pop operation on '" + SRCLANG_VALUE_TYPE_ID[int(SRCLANG_VALUE_GET_TYPE(args[0]))] +
                            "'").c_str()));
    }
}

SRCLANG_BUILTIN(clone) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    if (SRCLANG_VALUE_IS_OBJECT(args[0])) {
        switch (SRCLANG_VALUE_GET_TYPE(args[0])) {
            case ValueType::String:
            case ValueType::Error: {
                return SRCLANG_VALUE_STRING(strdup((char *) SRCLANG_VALUE_AS_OBJECT(args[0])->pointer));
            };
            case ValueType::List: {
                auto list = reinterpret_cast<SrcLangList *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
                auto new_list = new SrcLangList(list->begin(), list->end());
                return SRCLANG_VALUE_LIST(new_list);
            };
            case ValueType::Pointer: {
                auto object = SRCLANG_VALUE_AS_OBJECT(args[0]);
                if (object->is_ref) {
                    return args[0];
                }
                auto buffer = malloc(object->size);
                if (buffer == nullptr) {
                    throw std::runtime_error("can't malloc() for srclang::clone() " + std::string(strerror(errno)));
                }
                memcpy(buffer, object->pointer, object->size);
                auto value = SRCLANG_VALUE_POINTER(buffer);
                SRCLANG_VALUE_SET_SIZE(value, object->size);
                return value;
            }
                break;
            default:
                if (SRCLANG_VALUE_IS_OBJECT(args[0])) {
                    return SRCLANG_VALUE_ERROR(strdup(("invalid clone operation on '" +
                                                       SRCLANG_VALUE_TYPE_ID[int(SRCLANG_VALUE_GET_TYPE(args[0]))] +
                                                       "'").c_str()));
                }
        }
    }
    return args[0];
}

SRCLANG_BUILTIN(eval) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::String);
    const char *buffer = (const char *) SRCLANG_VALUE_AS_OBJECT(args[0])->pointer;
    auto result = interpreter->language->execute(buffer, "<inline>");
    if (SRCLANG_VALUE_GET_TYPE(result) == ValueType::Error) {
        // we are already registering object at language
        return builtin_clone({result}, interpreter);
    }
    return result;
}

SRCLANG_BUILTIN(alloc) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Number);
    long size = SRCLANG_VALUE_AS_NUMBER(args[0]);
    void *ptr = malloc(size * sizeof(unsigned char));
    if (ptr == nullptr) {
        std::stringstream ss;
        ss << "failed to allocate '" << size << "' size, " << strerror(errno);
        return SRCLANG_VALUE_ERROR(strdup(ss.str().c_str()));
    }
    auto value = SRCLANG_VALUE_POINTER(ptr);
    SRCLANG_VALUE_SET_SIZE(value, size);
    return value;
}

SRCLANG_BUILTIN(bound) {
    SRCLANG_CHECK_ARGS_EXACT(2);
    return SRCLANG_VALUE_BOUND(args[0], args[1]);
}

SRCLANG_BUILTIN(exit) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Number);

    auto status = (int) SRCLANG_VALUE_AS_NUMBER(args[0]);
    interpreter->grace_full_exit();
    exit(status);
}

SRCLANG_BUILTIN(free) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Pointer);
    auto object = SRCLANG_VALUE_AS_OBJECT(args[0]);
    if (object->cleanup == nullptr) {
        object->cleanup = free;
    }
    object->cleanup(object->pointer);
    object->pointer = nullptr;
    return SRCLANG_VALUE_TRUE;
}

SRCLANG_BUILTIN(open) {
    SRCLANG_CHECK_ARGS_RANGE(1, 2);
    const char *mode = "a+";
    if (args.size() == 2) {
        SRCLANG_CHECK_ARGS_TYPE(1, ValueType::String);
        mode = reinterpret_cast<const char *>(SRCLANG_VALUE_AS_OBJECT(args[1])->pointer);
    }

    FILE *fp = nullptr;
    switch (SRCLANG_VALUE_GET_TYPE(args[0])) {
        case ValueType::Number:
            fp = fdopen(SRCLANG_VALUE_AS_NUMBER(args[0]), mode);
            break;
        case ValueType::String:
            fp = fopen(reinterpret_cast<const char *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer), mode);
            break;
        case ValueType::Pointer:
            fp = reinterpret_cast<FILE *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
            break;
        default:
            SRCLANG_CHECK_ARGS_TYPE(0, ValueType::String);
    }
    if (fp == nullptr) {
        return SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_ERROR(strerror(errno)));
    }

    auto object = new SrcLangMap();
    auto value = SRCLANG_VALUE_MAP(object);

    object->insert({"__ptr__", SRCLANG_VALUE_SET_CLEANUP(
            SRCLANG_VALUE_POINTER(reinterpret_cast<void *>(fp)),
            +[](void *ptr) {
                auto fp = reinterpret_cast<FILE *>(ptr);
                fclose(fp);
            })});

    object->insert(
            {"Write", SRCLANG_VALUE_BOUND(
                    value,
                    SRCLANG_VALUE_BUILTIN_NEW(+[](std::vector<Value> &args) -> Value {
                        SRCLANG_CHECK_ARGS_EXACT(2);

                        SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Map);
                        SRCLANG_CHECK_ARGS_TYPE(1, ValueType::String);


                        auto self = reinterpret_cast<SrcLangMap *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
                        auto data = reinterpret_cast<const char *>(SRCLANG_VALUE_AS_OBJECT(args[1])->pointer);
                        auto fp = reinterpret_cast<FILE *>(SRCLANG_VALUE_AS_OBJECT(self->at("__ptr__"))->pointer);

                        int num = fwrite(data, sizeof(char), strlen(data), fp);
                        if (num == -1) {
                            return SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_ERROR(strerror(errno)));
                        }

                        return SRCLANG_VALUE_NUMBER(num);
                    }))});

    object->insert(
            {"Read", SRCLANG_VALUE_BOUND(
                    value,
                    SRCLANG_VALUE_BUILTIN_NEW(+[](std::vector<Value> &args) -> Value {
                        SRCLANG_CHECK_ARGS_EXACT(2);

                        SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Map);
                        SRCLANG_CHECK_ARGS_TYPE(1, ValueType::Number);

                        auto self = reinterpret_cast<SrcLangMap *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
                        int size = SRCLANG_VALUE_AS_NUMBER(args[1]);
                        auto fp = reinterpret_cast<FILE *>(SRCLANG_VALUE_AS_OBJECT(self->at("__ptr__"))->pointer);

                        auto buffer = new char[size + 1];

                        int num = fread(buffer, sizeof(char), size, fp);
                        if (num == -1) {
                            delete[] buffer;
                            return SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_ERROR(strerror(errno)));
                        }
                        buffer[size] = '\0';
                        return SRCLANG_VALUE_STRING(buffer);
                    }))});

    return value;
}

SRCLANG_BUILTIN(ffi) {
#ifndef FFI_SUPPORT
    return SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_ERROR("FFI NOT SUPPORTED"));
#else
    SRCLANG_CHECK_ARGS_EXACT(3);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Pointer);
    auto fun = SRCLANG_VALUE_AS_OBJECT(args[0])->pointer;

    SRCLANG_CHECK_ARGS_TYPE(1, ValueType::Pointer);
    auto returnType = reinterpret_cast<ffi_type *>(SRCLANG_VALUE_AS_OBJECT(args[1])->pointer);

    SRCLANG_CHECK_ARGS_TYPE(2, ValueType::List);
    auto parameters_list = reinterpret_cast<SrcLangList *>(SRCLANG_VALUE_AS_OBJECT(args[2])->pointer);

    auto parameters = new ffi_type *[parameters_list->size()];
    for (int i = 0; i < parameters_list->size(); i++) {
        parameters[i] = reinterpret_cast<ffi_type *>(SRCLANG_VALUE_AS_OBJECT(parameters_list->at(i))->pointer);
    }
    ffi_cif *cif = new ffi_cif();
    auto status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, parameters_list->size(), returnType, parameters);
    if (status != FFI_OK) {
        return SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_ERROR("failed to prepare cif"));
    }

    auto object = new SrcLangMap();
    auto value = SRCLANG_VALUE_MAP(object);

    object->insert({"__ptr__", SRCLANG_VALUE_SET_CLEANUP(
            SRCLANG_VALUE_POINTER(reinterpret_cast<void *>(cif)),
            +[](void *ptr) {
                delete reinterpret_cast<ffi_cif *>(ptr);
            })});
    object->insert({"__types__", SRCLANG_VALUE_SET_CLEANUP(
            SRCLANG_VALUE_POINTER(reinterpret_cast<void *>(parameters)),
            +[](void *ptr) {
                delete[] reinterpret_cast<ffi_type **>(ptr);
            })});

    object->insert({"__fun__", SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_POINTER(fun))});

    object->insert({"__call__",
                    SRCLANG_VALUE_BOUND(
                            value,
                            SRCLANG_VALUE_BUILTIN_NEW(+[](std::vector<Value> &args) -> Value {
                                SRCLANG_CHECK_ARGS_ATLEAST(1);
                                SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Map);
                                auto self = reinterpret_cast<SrcLangMap *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
                                auto cif = reinterpret_cast<ffi_cif *>(SRCLANG_VALUE_AS_OBJECT(
                                        self->at("__ptr__"))->pointer);
                                auto fun = SRCLANG_VALUE_AS_OBJECT(
                                        self->at("__fun__"))->pointer;

                                auto values = new void *[args.size() - 2];
                                for (int i = 2; i < args.size(); i++) {
                                    values[i - 2] = &SRCLANG_VALUE_AS_OBJECT(args[i])->pointer;
                                }

                                ffi_arg result;
                                ffi_call(cif, FFI_FN(fun), &result, values);
                                delete[]values;

                                return SRCLANG_VALUE_NUMBER(result);
                            }))});

    return value;
#endif
}

SRCLANG_BUILTIN(loadLibrary) {
    SRCLANG_CHECK_ARGS_EXACT(1);
    SRCLANG_CHECK_ARGS_TYPE(0, ValueType::String);
    auto library_id = reinterpret_cast<const char *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);

#ifdef _WIN32
    HMODULE handler = LoadLibrary(library_id);
    auto handler_value = SRCLANG_VALUE_SET_CLEANUP(SRCLANG_VALUE_POINTER(handler), [](void *ptr) {
        FreeLibrary(reinterpret_cast<HMODULE>(ptr));
    });
#else
    void* handler = dlopen(library_id, RTLD_LAZY | RTLD_LOCAL);
    auto value = SRCLANG_VALUE_SET_CLEANUP(SRCLANG_VALUE_POINTER(handler), [](void *ptr) {
        dlclosre(ptr);
    });
#endif

    auto object = new SrcLangMap();
    auto value = SRCLANG_VALUE_MAP(object);

    object->insert({"__ptr__", SRCLANG_VALUE_SET_CLEANUP(SRCLANG_VALUE_POINTER(handler), [](void *ptr) {
        FreeLibrary(reinterpret_cast<HMODULE>(ptr));
    })});

    object->insert({"getFun",
                    SRCLANG_VALUE_BOUND(
                            value,
                            SRCLANG_VALUE_BUILTIN_NEW(+[](std::vector<Value> &args) -> Value {
                                SRCLANG_CHECK_ARGS_ATLEAST(2);
                                SRCLANG_CHECK_ARGS_TYPE(0, ValueType::Map);
                                SRCLANG_CHECK_ARGS_TYPE(1, ValueType::String);
                                auto self = reinterpret_cast<SrcLangMap *>(SRCLANG_VALUE_AS_OBJECT(args[0])->pointer);
                                auto handler = SRCLANG_VALUE_AS_OBJECT(
                                        self->at("__ptr__"))->pointer;
                                auto fun = reinterpret_cast<const char *>(SRCLANG_VALUE_AS_OBJECT(
                                        args[1])->pointer);

                                return SRCLANG_VALUE_SET_REF(
                                        SRCLANG_VALUE_POINTER(GetProcAddress(reinterpret_cast<HMODULE>(handler), fun)));

                            }))});

    return value;
}

void srclang::define_constants(Language *language) {
#ifdef FFI_SUPPORT
#define FFI_TYPE_LIST \
    X(sint8)          \
    X(sint16)         \
    X(sint32)         \
    X(sint64)         \
    X(uint8)          \
    X(uint16)         \
    X(uint32)         \
    X(uint64)         \
    X(void)           \
    X(pointer)        \
    X(float)          \
    X(double)

#define X(id) language->define("ctype_"#id, SRCLANG_VALUE_SET_REF(SRCLANG_VALUE_POINTER(&ffi_type_##id)));
    FFI_TYPE_LIST
#undef X
#endif
}