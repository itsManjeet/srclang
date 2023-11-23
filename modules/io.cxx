#include "../src/Language/Language.hxx"
using namespace srclang;

SRCLANG_MODULE_FUNCTION_DEFINE(Open) {
}

SRCLANG_MODULE(io) {
    SRCLANG_MODULE_PROLOGUE(io);

    SRCLANG_MODULE_FUNCTION_ADD(io, Open);

    SRCLANG_MODULE_EPILOGUE(io);
}