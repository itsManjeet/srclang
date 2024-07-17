#include "ByteCode.h"

#include "SymbolTable.h"

using namespace SrcLang;

int ByteCode::debug(Instructions const& instructions,
        std::vector<Value> const& constants, int offset, std::wostream& os) {
    os << std::setfill(L'0') << std::setw(4) << offset << " ";
    auto op = static_cast<OpCode>(instructions[offset]);
    os << s2ws(SRCLANG_OPCODE_ID[static_cast<int>(op)]);
    offset += 1;
    switch (op) {
    case OpCode::CONST_: {
        auto const pos = instructions[offset++];
        if (!constants.empty()) {
            os << L" " << pos << L" '" << SRCLANG_VALUE_DEBUG(constants[pos])
               << L"'";
        }

    } break;
    case OpCode::INDEX:
    case OpCode::PACK:
    case OpCode::MAP: {
        os << " " << (int)instructions[offset++];
    } break;
    case OpCode::CONTINUE:
    case OpCode::BREAK:
    case OpCode::JNZ:
    case OpCode::JMP: {
        auto pos = instructions[offset++];
        os << " '" << pos << "'";
    } break;
    case OpCode::LOAD:
    case OpCode::STORE: {
        auto scope = instructions[offset++];
        auto pos = instructions[offset++];
        os << " " << pos << " '" << SRCLANG_SYMBOL_ID[scope] << "'";
    } break;
    case OpCode::CLOSURE: {
        auto constantIndex = instructions[offset++];
        auto nfree = instructions[offset++];
        os << L" " << constants[constantIndex] << L" " << nfree;
    } break;

    case OpCode::CONST_INT: {
        auto value = instructions[offset++];
        os << " '" << SRCLANG_VALUE_AS_NUMBER(value) << "'";
    } break;
    case OpCode::CALL: {
        auto count = instructions[offset++];
        os << " '" << count << "'";
    } break;
    default: offset += SRCLANG_OPCODE_SIZE[int(op)]; break;
    }

    return offset;
}
