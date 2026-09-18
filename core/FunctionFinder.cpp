#include "FunctionFinder.hpp"
#include "DebugSession.hpp"
#include <capstone/capstone.h>
#include <algorithm>

namespace edb_next {

std::vector<FunctionInfo> FunctionFinder::findFunctions(DebugSession& session, Address start, size_t scan_bytes) {
    std::vector<FunctionInfo> functions;
    if (scan_bytes == 0) return functions;

    auto code = session.readMemory(start, scan_bytes);
    if (code.empty()) return functions;

    csh cs_handle;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle) != CS_ERR_OK) {
        return functions;
    }

    cs_insn* insns = nullptr;
    size_t count = cs_disasm(cs_handle, code.data(), code.size(), start.value(), 0, &insns);
    if (count == 0) {
        cs_close(&cs_handle);
        return functions;
    }

    FunctionInfo currentFunc;
    bool inFunction = false;
    uint64_t maxForwardJumpTarget = 0;

    auto parseTarget = [](const std::string& op_str) -> uint64_t {
        if (op_str.empty()) return 0;
        char* end = nullptr;
        uint64_t target = std::strtoull(op_str.c_str(), &end, 0);
        return (end != op_str.c_str()) ? target : 0;
    };

    for (size_t i = 0; i < count; ++i) {
        const auto& in = insns[i];
        Address inAddr(in.address);
        std::string mnem = in.mnemonic;
        std::string op = in.op_str;

        // Check if exact symbol at this address
        auto symOpt = session.symbols().findExactSymbol(inAddr);
        bool isSymbolFunc = symOpt.has_value() && (symOpt->type == 2 || symOpt->size > 0);

        bool isPrologue = (mnem == "endbr64") ||
                          (mnem == "push" && op == "rbp") ||
                          isSymbolFunc;

        if (!inFunction) {
            if (isPrologue) {
                inFunction = true;
                maxForwardJumpTarget = 0;
                currentFunc = FunctionInfo{};
                currentFunc.startAddress = inAddr;
                currentFunc.hasPrologue = (mnem == "push" && op == "rbp") || (mnem == "endbr64");
                if (symOpt) {
                    currentFunc.name = symOpt->name;
                } else {
                    currentFunc.name = "sub_" + inAddr.toHex().substr(2);
                }
            }
        } else {
            // Already inside a function, but hit a new symbol function start
            if (isSymbolFunc && inAddr != currentFunc.startAddress) {
                currentFunc.endAddress = inAddr;
                currentFunc.size = currentFunc.endAddress.value() - currentFunc.startAddress.value();
                functions.push_back(currentFunc);

                currentFunc = FunctionInfo{};
                currentFunc.startAddress = inAddr;
                currentFunc.name = symOpt->name;
                currentFunc.hasPrologue = true;
                maxForwardJumpTarget = 0;
            }
        }

        if (inFunction) {
            // Track forward jump targets to handle early returns
            if (!mnem.empty() && mnem[0] == 'j') {
                uint64_t target = parseTarget(op);
                if (target > inAddr.value() && (target - inAddr.value() <= 65536)) {
                    maxForwardJumpTarget = std::max(maxForwardJumpTarget, target);
                }
            }

            if (mnem == "ret") {
                currentFunc.hasEpilogue = true;
                // If no remaining forward jump crosses past this return, terminate function
                if (inAddr.value() >= maxForwardJumpTarget) {
                    currentFunc.endAddress = inAddr + in.size;
                    currentFunc.size = currentFunc.endAddress.value() - currentFunc.startAddress.value();
                    functions.push_back(currentFunc);
                    inFunction = false;
                    maxForwardJumpTarget = 0;
                }
            }
        }
    }

    if (inFunction) {
        currentFunc.endAddress = Address(insns[count - 1].address + insns[count - 1].size);
        currentFunc.size = currentFunc.endAddress.value() - currentFunc.startAddress.value();
        functions.push_back(currentFunc);
    }

    cs_free(insns, count);
    cs_close(&cs_handle);
    return functions;
}

std::optional<FunctionInfo> FunctionFinder::findEnclosingFunction(DebugSession& session, Address addr) {
    // 1. Fast path: check ELF symbol table
    if (auto nearSym = session.symbols().findNearestSymbol(addr)) {
        if (nearSym->first.type == 2 && nearSym->first.size > 0) { // STT_FUNC
            FunctionInfo info;
            info.startAddress = nearSym->first.address;
            info.endAddress = nearSym->first.address + nearSym->first.size;
            info.size = nearSym->first.size;
            info.name = nearSym->first.name;
            info.hasPrologue = true;
            info.hasEpilogue = true;
            return info;
        }
    }

    // 2. Scan window around addr
    uint64_t window_before = 2048;
    Address scan_start = (addr.value() > window_before) ? (addr - window_before) : Address(0);
    size_t scan_size = 4096;

    auto funcs = findFunctions(session, scan_start, scan_size);
    for (const auto& f : funcs) {
        if (addr >= f.startAddress && addr < f.endAddress) {
            return f;
        }
    }

    return std::nullopt;
}

} // namespace edb_next
