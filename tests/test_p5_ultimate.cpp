#include "core/UserfaultFdEngine.hpp"
#include "core/BtfParser.hpp"
#include "core/BreakpointManager.hpp"
#include "core/PageGuardManager.hpp"
#include "core/AntiAntiDebug.hpp"
#include "core/MemoryScanner.hpp"
#include "core/LinuxDebugEngine.hpp"
#include "tests/MockDebugBackend.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>


using namespace edb_next;

static void test_userfaultfd_engine() {
    std::cout << "[TEST] Running test_userfaultfd_engine..." << std::endl;

    UserfaultFdEngine uffd;
    bool ok = uffd.initialize();
    assert(ok);
    assert(uffd.isInitialized());

    std::cout << "  [Info] userfaultfd system support: "
              << (UserfaultFdEngine::isSystemSupported() ? "YES" : "NO (Sandbox simulation fallback)")
              << ", isSimulated=" << (uffd.isSimulated() ? "YES" : "NO") << std::endl;

    Address watchedAddr(0x55550000);
    bool regOk = uffd.registerRange(watchedAddr, 4096, UffdFaultMode::WriteProtect, "Test Region");
    assert(regOk);
    assert(uffd.isAddressWatched(watchedAddr));
    assert(uffd.isAddressWatched(Address(0x55550100)));
    assert(!uffd.isAddressWatched(Address(0x60000000)));

    auto region = uffd.findWatchedRegion(watchedAddr);
    assert(region.has_value());
    assert(region->size == 4096);
    assert(region->comment == "Test Region");

    // Test fault event queuing and polling
    UffdFaultEvent ev{
        .faultAddress = watchedAddr + 0x20,
        .isWrite = true,
        .isWriteProtected = true,
        .threadId = 1234,
        .timestamp = std::chrono::system_clock::now(),
        .details = "Simulated fault test"
    };
    uffd.simulateFault(ev);

    auto events = uffd.pollEvents(0);
    assert(events.size() == 1);
    assert(events[0].faultAddress == watchedAddr + 0x20);
    assert(events[0].isWrite);
    assert(events[0].isWriteProtected);

    // Verify hit count incremented
    auto regAfter = uffd.findWatchedRegion(watchedAddr);
    assert(regAfter.has_value());
    assert(regAfter->hitCount == 1);

    // Resolve fault
    bool resOk = uffd.resolveFault(watchedAddr + 0x20);
    assert(resOk);

    // Unregister range
    bool unregOk = uffd.unregisterRange(watchedAddr);
    assert(unregOk);
    assert(!uffd.isAddressWatched(watchedAddr));

    std::cout << "  -> test_userfaultfd_engine PASSED" << std::endl;
}

static void test_btf_parser_synthetic_and_vmlinux() {
    std::cout << "[TEST] Running test_btf_parser_synthetic_and_vmlinux..." << std::endl;

    BtfParser parser;

    // 1. Test synthetic BTF binary creation and parsing
    std::vector<std::pair<std::string, uint32_t>> fields = {
        {"device_id", 4},
        {"status_code", 4},
        {"flags", 4}
    };
    auto syntheticBtf = BtfParser::createSyntheticBtf("SampleDevice", fields);
    assert(!syntheticBtf.empty());

    bool parseOk = parser.parseBuffer(syntheticBtf);
    assert(parseOk);
    assert(!parser.empty());

    const auto* entry = parser.findType("SampleDevice");
    assert(entry != nullptr);
    assert(entry->name == "SampleDevice");
    assert(entry->size == 12);
    assert(entry->fields.size() == 3);
    assert(entry->fields[0].name == "device_id");
    assert(entry->fields[0].offset == 0);
    assert(entry->fields[1].name == "status_code");
    assert(entry->fields[1].offset == 4);
    assert(entry->fields[2].name == "flags");
    assert(entry->fields[2].offset == 8);

    // 2. Export to TypeManager
    TypeManager typeMgr;
    size_t exported = parser.exportToTypeManager(typeMgr);
    assert(exported >= 1);
    const auto* def = typeMgr.findStruct("SampleDevice");
    assert(def != nullptr);
    assert(def->fields.size() == 3);
    assert(def->totalSize == 12);

    std::cout << "  [Info] Synthetic BTF successfully parsed and registered struct 'SampleDevice' (size "
              << def->totalSize << " bytes)." << std::endl;

    // 3. Test host kernel BTF if available
    if (std::filesystem::exists("/sys/kernel/btf/vmlinux")) {
        BtfParser kernelBtf;
        bool vmlinuxOk = kernelBtf.parseVmlinux("/sys/kernel/btf/vmlinux");
        if (vmlinuxOk) {
            std::cout << "  [Info] Successfully parsed /sys/kernel/btf/vmlinux: discovered "
                      << kernelBtf.typeCount() << " total types ("
                      << kernelBtf.structAndUnionNames().size() << " structs/unions)." << std::endl;
            assert(kernelBtf.typeCount() > 100);
        } else {
            std::cout << "  [Notice] /sys/kernel/btf/vmlinux not readable under current permissions (skipped)." << std::endl;
        }
    }

    std::cout << "  -> test_btf_parser_synthetic_and_vmlinux PASSED" << std::endl;
}

static void test_dr6_precision_attribution_and_pageguard_fallback() {
    std::cout << "[TEST] Running test_dr6_precision_attribution_and_pageguard_fallback..." << std::endl;

    // 1. DR6 status parsing
    Dr6Status dr6 = Dr6Status::fromRaw(0x4001); // Slot 0 hit + single step (BS)
    assert(dr6.slot0Hit);
    assert(!dr6.slot1Hit);
    assert(!dr6.slot2Hit);
    assert(!dr6.slot3Hit);
    assert(dr6.singleStepHit);
    assert(dr6.hitSlot() == 0);
    assert(dr6.anySlotHit());

    Dr6Status dr6Slot2 = Dr6Status::fromRaw(0x0004); // Slot 2 hit
    assert(dr6Slot2.slot2Hit);
    assert(dr6Slot2.hitSlot() == 2);

    // 2. Hardware slot exhaustion and PageGuard automatic fallback
    std::vector<uint8_t> mockMem(0x10000, 0x90);
    std::array<bool, 4> hwSlotsSet{false, false, false, false};

    auto read_fn = [&](Address a, void* b, size_t s) {
        if (a.value() + s <= mockMem.size()) {
            std::memcpy(b, mockMem.data() + a.value(), s);
            return true;
        }
        return false;
    };
    auto write_fn = [&](Address a, const void* b, size_t s) {
        if (a.value() + s <= mockMem.size()) {
            std::memcpy(mockMem.data() + a.value(), b, s);
            return true;
        }
        return false;
    };
    auto set_hw_fn = [&](int slot, Address, HardwareBpType, HardwareBpSize) {
        if (slot >= 0 && slot < 4) {
            hwSlotsSet[slot] = true;
            return true;
        }
        return false;
    };
    auto clear_hw_fn = [&](int slot) {
        if (slot >= 0 && slot < 4) {
            hwSlotsSet[slot] = false;
            return true;
        }
        return false;
    };

    BreakpointManager bpMgr(read_fn, write_fn, set_hw_fn, clear_hw_fn);
    PageGuardManager pgMgr([](Address, size_t, int) { return true; });
    bpMgr.setPageGuardManager(&pgMgr);
    bpMgr.setAutoFallbackToPageGuard(true);

    // Add 4 hardware breakpoints to fill slots 0..3
    assert(bpMgr.addHardwareBreakpoint(Address(0x1000), HardwareBpType::Write, HardwareBpSize::Byte4, "hw0"));
    assert(bpMgr.addHardwareBreakpoint(Address(0x2000), HardwareBpType::Write, HardwareBpSize::Byte4, "hw1"));
    assert(bpMgr.addHardwareBreakpoint(Address(0x3000), HardwareBpType::Write, HardwareBpSize::Byte4, "hw2"));
    assert(bpMgr.addHardwareBreakpoint(Address(0x4000), HardwareBpType::Write, HardwareBpSize::Byte4, "hw3"));

    // Slot 5: Hardware registers exhausted -> automatic fallback to PageGuard!
    assert(bpMgr.addHardwareBreakpoint(Address(0x5000), HardwareBpType::Write, HardwareBpSize::Byte4, "hw_fallback"));
    assert(bpMgr.hasBreakpoint(Address(0x5000)));

    const auto* bpFallback = bpMgr.getBreakpoint(Address(0x5000));
    assert(bpFallback != nullptr);
    assert(bpFallback->isPageGuardFallback);
    assert(bpFallback->hardwareSlot == -1);
    assert(pgMgr.hasGuard(Address(0x5000)));

    std::cout << "  [Info] Breakpoint 5 successfully fell back to PageGuard: "
              << bpFallback->symbol << " at " << bpFallback->address.toHex() << std::endl;

    // Test DR6 attribution for Slot 1 hit
    auto hitAddr = bpMgr.attributeDr6(Dr6Status::fromRaw(0x0002));
    assert(hitAddr.has_value());
    assert(*hitAddr == Address(0x2000));
    std::cout << "  [Info] DR6 accurately attributed Slot 1 to " << hitAddr->toHex() << std::endl;

    // Removing fallback breakpoint unregisters page guard
    assert(bpMgr.removeBreakpoint(Address(0x5000)));
    assert(!pgMgr.hasGuard(Address(0x5000)));

    std::cout << "  -> test_dr6_precision_attribution_and_pageguard_fallback PASSED" << std::endl;
}

static void test_anti_anti_debug_engine() {
    std::cout << "[TEST] Running test_anti_anti_debug_engine..." << std::endl;

    // 1. Test TracerPid spoofing
    std::string fakeStatus =
        "Name:\ttest_target\n"
        "State:\tS (sleeping)\n"
        "Tgid:\t4321\n"
        "Ngid:\t0\n"
        "Pid:\t4321\n"
        "PPid:\t4000\n"
        "TracerPid:\t99999\n"
        "Uid:\t1000\t1000\t1000\t1000\n";

    std::string cleanStatus = AntiAntiDebugEngine::sanitizeProcStatus(fakeStatus);
    assert(cleanStatus.find("TracerPid:\t0\n") != std::string::npos);
    assert(cleanStatus.find("99999") == std::string::npos);
    std::cout << "  [Info] TracerPid correctly sanitized from 99999 to 0." << std::endl;

    // 2. Test RDTSC cycle smoothing
    AntiAntiDebugEngine engine;
    uint64_t tsc0 = 1000000;
    uint64_t filtered0 = engine.filterRdtscCycles(tsc0);
    assert(filtered0 == tsc0);

    // Large jump: 300 million cycles later (typical of single-step debugger pause)
    uint64_t tsc1 = tsc0 + 300'000'000;
    uint64_t filtered1 = engine.filterRdtscCycles(tsc1);
    uint64_t delta = filtered1 - filtered0;
    assert(delta <= engine.config().maxSyntheticRdtscDelta);
    assert(engine.detectionCount() >= 1);
    std::cout << "  [Info] 300M cycle delay smoothed to " << delta
              << " cycles (detection recorded: " << engine.allDetections()[0].type << ")." << std::endl;

    // 3. Syscall anti-debug probe interception
    bool ptraceProbe = engine.inspectSyscall(Address(0x401000), SYS_ptrace, PTRACE_TRACEME, 0);
    assert(ptraceProbe);

    bool prctlProbe = engine.inspectSyscall(Address(0x401020), SYS_prctl, PR_SET_DUMPABLE, 0);
    assert(prctlProbe);


    assert(engine.detectionCount() == 3);

    std::cout << "  -> test_anti_anti_debug_engine PASSED" << std::endl;
}

static void test_neon_and_vector_scanner() {
    std::cout << "[TEST] Running test_neon_and_vector_scanner..." << std::endl;

    std::cout << "  [Info] Active SIMD Scanner Engine: " << MemoryScanner::activeSimdEngineName() << std::endl;
    std::cout << "  [Info] AVX2 Available: " << (MemoryScanner::isAvx2Supported() ? "YES" : "NO") << std::endl;
    std::cout << "  [Info] ARM NEON Available: " << (MemoryScanner::isNeonSupported() ? "YES" : "NO") << std::endl;

    MockDebugBackend backend;
    backend.attached_ = true;
    backend.pid_ = 1234;

    std::vector<uint8_t> mem(1024 * 1024, 0); // 1MB buffer

    // Place target int32 0x1337BEEF at offset 0x4000 and 0x8000
    int32_t targetVal = 0x1337BEEF;
    std::memcpy(mem.data() + 0x4000, &targetVal, sizeof(targetVal));
    std::memcpy(mem.data() + 0x8000, &targetVal, sizeof(targetVal));

    Address base(0x1000000);
    MemoryRegion r;
    r.start = base;
    r.end = base + mem.size();
    r.permissions = "rw-p";
    backend.fakeRegions_.push_back(r);


    backend.onReadMemory_ = [base, &mem](Address addr, void* buf, size_t sz) {
        if (addr >= base && (addr.value() - base.value() + sz) <= mem.size()) {
            size_t off = addr.value() - base.value();
            std::memcpy(buf, mem.data() + off, sz);
            return true;
        }
        return false;
    };

    MemoryScanner scanner;
    ScanOptions opt;
    opt.dataType = ScanDataType::Int32;
    opt.compareType = ScanCompareType::ExactValue;
    opt.valueStr = "0x1337BEEF";
    opt.alignment = 4;
    opt.customStart = base;
    opt.customEnd = base + 0x100000;

    size_t found = scanner.firstScan(backend, opt);
    assert(found == 2);
    assert(scanner.results()[0].address == Address(base.value() + 0x4000));
    assert(scanner.results()[1].address == Address(base.value() + 0x8000));


    std::cout << "  [Info] MemoryScanner found 2 occurrences of 0x1337BEEF at expected offsets." << std::endl;
    std::cout << "  -> test_neon_and_vector_scanner PASSED" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   edb-next Ultimate Next-Gen (P5) Test " << std::endl;
    std::cout << "========================================" << std::endl;

    test_userfaultfd_engine();
    test_btf_parser_synthetic_and_vmlinux();
    test_dr6_precision_attribution_and_pageguard_fallback();
    test_anti_anti_debug_engine();
    test_neon_and_vector_scanner();

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL NEXT-GEN (P5) TESTS PASSED!       " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
