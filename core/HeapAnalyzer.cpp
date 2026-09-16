#include "HeapAnalyzer.hpp"
#include "DebugSession.hpp"
#include <algorithm>

namespace edb_next {

std::vector<HeapChunkInfo> HeapAnalyzer::analyze(DebugSession& session, size_t max_chunks) {
    std::vector<HeapChunkInfo> chunks;
    auto regions = session.memoryRegions();

    // 1. Locate [heap] region
    const MemoryRegion* heapReg = nullptr;
    for (const auto& reg : regions) {
        if (reg.pathname == "[heap]") {
            heapReg = &reg;
            break;
        }
    }

    if (!heapReg || !heapReg->isReadable()) return chunks;

    Address heapStart = heapReg->start;
    Address heapEnd = heapReg->end;
    uint64_t heapSize = heapReg->size();
    if (heapSize < 0x40) return chunks;

    // 2. Find first chunk header within the first page of heap
    Address curr = heapStart;
    bool foundFirst = false;

    // Scan up to 4KB for first valid chunk
    size_t scanLimit = std::min<size_t>(heapSize, 4096);
    auto initBuf = session.readMemory(heapStart, scanLimit);
    if (initBuf.size() < 16) return chunks;

    for (size_t offset = 0; offset + 16 <= initBuf.size(); offset += 8) {
        uint64_t rawSize = 0;
        std::memcpy(&rawSize, &initBuf[offset + 8], sizeof(rawSize));
        uint64_t realSize = rawSize & ~0x7ULL;

        if (realSize >= 0x20 && (realSize % 0x10 == 0) && realSize <= heapSize) {
            // Verify next chunk size if within initBuf
            if (offset + realSize + 16 <= initBuf.size()) {
                uint64_t nextRawSize = 0;
                std::memcpy(&nextRawSize, &initBuf[offset + realSize + 8], sizeof(nextRawSize));
                uint64_t nextRealSize = nextRawSize & ~0x7ULL;
                if (nextRealSize >= 0x20 && (nextRealSize % 0x10 == 0)) {
                    curr = heapStart + offset;
                    foundFirst = true;
                    break;
                }
            } else {
                curr = heapStart + offset;
                foundFirst = true;
                break;
            }
        }
    }

    if (!foundFirst) {
        // Fallback: assume offset 0x290 (common for glibc tcache) or 0x0
        curr = heapStart;
    }

    // 3. Walk the chunk chain
    while (curr + 16 <= heapEnd && chunks.size() < max_chunks) {
        auto header = session.readMemory(curr, 16);
        if (header.size() < 16) break;

        uint64_t prevSize = 0;
        uint64_t rawSize = 0;
        std::memcpy(&prevSize, &header[0], sizeof(prevSize));
        std::memcpy(&rawSize, &header[8], sizeof(rawSize));

        uint64_t realSize = rawSize & ~0x7ULL;
        if (realSize < 0x20 || (realSize % 0x10 != 0)) {
            // Reached top chunk or corrupted chunk
            break;
        }

        HeapChunkInfo info;
        info.chunkAddress = curr;
        info.userAddress = curr + 16;
        info.prevSize = prevSize;
        info.rawSize = rawSize;
        info.actualSize = realSize;
        info.flagPrevInUse = (rawSize & 0x1) != 0;
        info.flagIsMmapped = (rawSize & 0x2) != 0;
        info.flagNonMainArena = (rawSize & 0x4) != 0;

        // Check if top chunk (extends towards heapEnd)
        if (curr + realSize >= heapEnd - 0x20) {
            info.isTopChunk = true;
            info.status = "Top Chunk";
            chunks.push_back(info);
            break;
        }

        // Check next chunk to determine if this chunk is allocated or free
        Address nextAddr = curr + realSize;
        if (nextAddr + 16 <= heapEnd) {
            auto nextHeader = session.readMemory(nextAddr, 16);
            if (nextHeader.size() >= 16) {
                uint64_t nextRaw = 0;
                std::memcpy(&nextRaw, &nextHeader[8], sizeof(nextRaw));
                bool nextPinuse = (nextRaw & 0x1) != 0;
                info.status = nextPinuse ? "Allocated" : "Free";
            } else {
                info.status = "Allocated";
            }
        } else {
            info.status = "Allocated";
        }

        chunks.push_back(info);
        curr = nextAddr;
    }

    return chunks;
}

} // namespace edb_next
