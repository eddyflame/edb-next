#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace edb_next {

class Assembler {
public:
    // Assembles x86-64 Intel syntax assembly string (e.g. "xor eax, eax", "nop", "mov rdi, [rbp - 8]")
    // into machine code bytes.
    // origin: address where instruction is located (relevant for relative jumps like jmp/call)
    static Result<std::vector<uint8_t>> assemble(const std::string& instruction, Address origin = Address(0));
};

} // namespace edb_next
