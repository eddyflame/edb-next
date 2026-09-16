#pragma once

#include "Types.hpp"
#include <vector>
#include <string>

namespace edb_next {

class DebugSession;

struct HeapChunkInfo {
    Address chunkAddress{0};
    Address userAddress{0};
    uint64_t prevSize{0};
    uint64_t rawSize{0};
    uint64_t actualSize{0};
    bool flagPrevInUse{false};
    bool flagIsMmapped{false};
    bool flagNonMainArena{false};
    bool isTopChunk{false};
    std::string status; // "Allocated", "Free (Suspected)", "Top Chunk"
};

class HeapAnalyzer {
public:
    static std::vector<HeapChunkInfo> analyze(DebugSession& session, size_t max_chunks = 2000);
};

} // namespace edb_next
