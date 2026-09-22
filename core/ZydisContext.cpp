#include "ZydisContext.hpp"
#include <cctype>
#include <cstdio>

#if defined(HAVE_ZYDIS)
#include <Zydis/Zydis.h>
#endif

namespace edb_next {

#if defined(HAVE_ZYDIS)
namespace {

struct ZydisThreadState {
    ZydisDecoder decoder;
    ZydisFormatter formatterIntel;
    ZydisFormatter formatterATT;
    bool initialized{false};

    void ensureInit() {
        if (!initialized) {
            ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
            ZydisFormatterInit(&formatterIntel, ZYDIS_FORMATTER_STYLE_INTEL);
            ZydisFormatterInit(&formatterATT, ZYDIS_FORMATTER_STYLE_ATT);
            initialized = true;
        }
    }
};

static thread_local ZydisThreadState t_state;

} // namespace
#endif

bool ZydisContext::isAvailable() noexcept {
#if defined(HAVE_ZYDIS)
    return true;
#else
    return false;
#endif
}

FastInstructionInfo ZydisContext::decodeFast(const uint8_t* code, size_t size, Address runtimeAddr) noexcept {
    FastInstructionInfo info;
    info.address = runtimeAddr;

#if defined(HAVE_ZYDIS)
    if (!code || size == 0) return info;

    t_state.ensureInit();

    ZydisDecodedInstruction insn;
    ZydisDecoderContext context;
    ZyanStatus status = ZydisDecoderDecodeInstruction(&t_state.decoder, &context, code, size, &insn);
    if (!ZYAN_SUCCESS(status)) {
        return info;
    }

    info.isValid = true;
    info.length = static_cast<uint8_t>(insn.length);
    info.isCall = (insn.meta.category == ZYDIS_CATEGORY_CALL);
    info.isRet = (insn.meta.category == ZYDIS_CATEGORY_RET);
    info.isBranch = (insn.meta.category == ZYDIS_CATEGORY_COND_BR || insn.meta.category == ZYDIS_CATEGORY_UNCOND_BR);
    info.isConditional = (insn.meta.category == ZYDIS_CATEGORY_COND_BR);
    info.isSyscall = (insn.meta.category == ZYDIS_CATEGORY_SYSCALL || insn.mnemonic == ZYDIS_MNEMONIC_SYSENTER);
    info.isRep = ((insn.attributes & (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE | ZYDIS_ATTRIB_HAS_REPNE)) != 0);

    const char* mnemStr = ZydisMnemonicGetString(insn.mnemonic);
    if (mnemStr) {
        info.mnemonic = mnemStr;
    }

    // If branch or call, compute branch target if operand is immediate/relative
    if (info.isBranch || info.isCall) {
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        if (ZYAN_SUCCESS(ZydisDecoderDecodeOperands(&t_state.decoder, &context, &insn, operands, insn.operand_count_visible))) {
            if (insn.operand_count_visible > 0) {
                ZyanU64 target = 0;
                if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&insn, &operands[0], runtimeAddr.value(), &target))) {
                    info.branchTarget = Address(target);
                }
            }
        }
    }
#endif

    return info;
}

std::vector<DisassembledInstruction> ZydisContext::disassemble(
    const uint8_t* code,
    size_t size,
    Address startAddr,
    size_t count,
    DisassemblySyntax syntax,
    bool uppercaseMnemonics,
    bool simplifyRipRel) {

    std::vector<DisassembledInstruction> result;

#if defined(HAVE_ZYDIS)
    if (!code || size == 0 || count == 0) return result;

    t_state.ensureInit();

    result.reserve(count);
    ZydisFormatter* formatter = (syntax == DisassemblySyntax::ATT) ? &t_state.formatterATT : &t_state.formatterIntel;

    // Configure properties
    ZydisFormatterSetProperty(formatter, ZYDIS_FORMATTER_PROP_FORCE_RELATIVE_RIPREL, simplifyRipRel ? ZYAN_FALSE : ZYAN_TRUE);
    ZydisFormatterSetProperty(formatter, ZYDIS_FORMATTER_PROP_UPPERCASE_MNEMONIC, uppercaseMnemonics ? ZYAN_TRUE : ZYAN_FALSE);
    ZydisFormatterSetProperty(formatter, ZYDIS_FORMATTER_PROP_UPPERCASE_REGISTERS, uppercaseMnemonics ? ZYAN_TRUE : ZYAN_FALSE);
    ZydisFormatterSetProperty(formatter, ZYDIS_FORMATTER_PROP_UPPERCASE_TYPECASTS, uppercaseMnemonics ? ZYAN_TRUE : ZYAN_FALSE);

    ZyanUSize offset = 0;
    ZydisDecodedInstruction insn;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    while (offset < size && result.size() < count) {
        ZyanStatus status = ZydisDecoderDecodeFull(&t_state.decoder, code + offset, size - offset, &insn, operands);
        if (!ZYAN_SUCCESS(status)) {
            // Illegal / unmapped byte fallback: format single byte as db 0xXX
            DisassembledInstruction fallback;
            fallback.address = startAddr + offset;
            fallback.mnemonic = uppercaseMnemonics ? "DB" : "db";
            char hex_buf[16];
            std::snprintf(hex_buf, sizeof(hex_buf), "0x%02x", code[offset]);
            fallback.operands = hex_buf;
            fallback.bytes = {code[offset]};
            result.push_back(std::move(fallback));
            offset += 1;
            continue;
        }

        Address insnAddr = startAddr + offset;
        std::vector<uint8_t> insnBytes(code + offset, code + offset + insn.length);

        // Format mnemonic
        char mnemBuf[64];
        if (uppercaseMnemonics) {
            std::string m = ZydisMnemonicGetString(insn.mnemonic);
            for (char& c : m) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            std::snprintf(mnemBuf, sizeof(mnemBuf), "%s", m.c_str());
        } else {
            const char* m = ZydisMnemonicGetString(insn.mnemonic);
            std::snprintf(mnemBuf, sizeof(mnemBuf), "%s", m ? m : "???");
        }

        // Format operands
        std::string opStr;
        for (ZyanU8 i = 0; i < insn.operand_count_visible; ++i) {
            char opBuf[256];
            if (ZYAN_SUCCESS(ZydisFormatterFormatOperand(formatter, &insn, &operands[i], opBuf, sizeof(opBuf), insnAddr.value(), nullptr))) {
                if (!opStr.empty()) opStr += ", ";
                opStr += opBuf;
            }
        }

        result.push_back(DisassembledInstruction{
            .address = insnAddr,
            .mnemonic = mnemBuf,
            .operands = std::move(opStr),
            .bytes = std::move(insnBytes),
            .symbol = {},
            .isCurrentRip = false,
            .hasBreakpoint = false,
            .isBreakpointEnabled = true,
            .sourceFile = {},
            .sourceFullPath = {},
            .sourceLine = 0,
            .sourceText = {},
            .isSourceLineStart = false
        });

        offset += insn.length;
    }
#endif

    return result;
}

} // namespace edb_next
