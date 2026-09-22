#include "CapstoneContext.hpp"

namespace edb_next {

namespace {

struct ThreadCapstoneState {
    csh basicHandle{0};
    bool basicInUse{false};
    DisassemblySyntax basicSyntax{DisassemblySyntax::Intel};

    csh detailHandle{0};
    bool detailInUse{false};
    DisassemblySyntax detailSyntax{DisassemblySyntax::Intel};

    ~ThreadCapstoneState() {
        if (basicHandle != 0) {
            cs_close(&basicHandle);
            basicHandle = 0;
        }
        if (detailHandle != 0) {
            cs_close(&detailHandle);
            detailHandle = 0;
        }
    }
};

static thread_local ThreadCapstoneState t_state;

} // namespace

CapstoneLease::~CapstoneLease() {
    reset();
}

CapstoneLease& CapstoneLease::operator=(CapstoneLease&& other) noexcept {
    if (this != &other) {
        reset();
        handle_ = other.handle_;
        source_ = other.source_;
        other.handle_ = 0;
    }
    return *this;
}

void CapstoneLease::reset() noexcept {
    if (handle_ == 0) return;

    switch (source_) {
        case Source::BasicPool:
            t_state.basicInUse = false;
            break;
        case Source::DetailPool:
            t_state.detailInUse = false;
            break;
        case Source::Standalone:
            cs_close(&handle_);
            break;
    }
    handle_ = 0;
}

CapstoneLease CapstoneContext::acquire(
    bool detail,
    DisassemblySyntax syntax,
    cs_mode mode) {

    // Only cache the standard x86_64 64-bit handles in thread-local pool
    if (mode == CS_MODE_64) {
        if (!detail) {
            if (!t_state.basicInUse) {
                if (t_state.basicHandle == 0) {
                    if (cs_open(CS_ARCH_X86, CS_MODE_64, &t_state.basicHandle) != CS_ERR_OK) {
                        return CapstoneLease();
                    }
                    t_state.basicSyntax = DisassemblySyntax::Intel;
                    cs_option(t_state.basicHandle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
                }

                if (t_state.basicSyntax != syntax) {
                    cs_option(t_state.basicHandle, CS_OPT_SYNTAX,
                              syntax == DisassemblySyntax::ATT ? CS_OPT_SYNTAX_ATT : CS_OPT_SYNTAX_INTEL);
                    t_state.basicSyntax = syntax;
                }

                t_state.basicInUse = true;
                return CapstoneLease(t_state.basicHandle, CapstoneLease::Source::BasicPool);
            }
        } else {
            if (!t_state.detailInUse) {
                if (t_state.detailHandle == 0) {
                    if (cs_open(CS_ARCH_X86, CS_MODE_64, &t_state.detailHandle) != CS_ERR_OK) {
                        return CapstoneLease();
                    }
                    cs_option(t_state.detailHandle, CS_OPT_DETAIL, CS_OPT_ON);
                    t_state.detailSyntax = DisassemblySyntax::Intel;
                    cs_option(t_state.detailHandle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);
                }

                if (t_state.detailSyntax != syntax) {
                    cs_option(t_state.detailHandle, CS_OPT_SYNTAX,
                              syntax == DisassemblySyntax::ATT ? CS_OPT_SYNTAX_ATT : CS_OPT_SYNTAX_INTEL);
                    t_state.detailSyntax = syntax;
                }

                t_state.detailInUse = true;
                return CapstoneLease(t_state.detailHandle, CapstoneLease::Source::DetailPool);
            }
        }
    }

    // Reentrant on the same thread or custom architecture mode: open standalone
    csh standalone = 0;
    if (cs_open(CS_ARCH_X86, mode, &standalone) != CS_ERR_OK) {
        return CapstoneLease();
    }
    if (detail) {
        cs_option(standalone, CS_OPT_DETAIL, CS_OPT_ON);
    }
    cs_option(standalone, CS_OPT_SYNTAX,
              syntax == DisassemblySyntax::ATT ? CS_OPT_SYNTAX_ATT : CS_OPT_SYNTAX_INTEL);

    return CapstoneLease(standalone, CapstoneLease::Source::Standalone);
}

} // namespace edb_next
