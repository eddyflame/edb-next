#include "TypeManager.hpp"
#include "ClangAstParser.hpp"
#include "IDebugBackend.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <cctype>

namespace edb_next {

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string cleanComments(const std::string& code) {
    std::string result;
    bool inLineComment = false;
    bool inBlockComment = false;
    for (size_t i = 0; i < code.size(); ++i) {
        if (inLineComment) {
            if (code[i] == '\n') {
                inLineComment = false;
                result += '\n';
            }
        } else if (inBlockComment) {
            if (code[i] == '*' && i + 1 < code.size() && code[i + 1] == '/') {
                inBlockComment = false;
                ++i;
            }
        } else {
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
                inLineComment = true;
                ++i;
            } else if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
                inBlockComment = true;
                ++i;
            } else {
                result += code[i];
            }
        }
    }
    return result;
}

void stripComments(std::string& s) {
    auto cpos = s.find("//");
    if (cpos != std::string::npos) {
        s = s.substr(0, cpos);
    }
}

template<typename T>
T readValAt(const uint8_t* mem) {
    T val;
    std::memcpy(&val, mem, sizeof(T));
    return val;
}

} // anonymous namespace

std::string StructDefinition::formatFieldValue(const StructField& field, const uint8_t* mem, size_t memLen) const {
    if (field.offset >= memLen) return "<out of bounds>";
    const uint8_t* ptr = mem + field.offset;
    size_t avail = memLen - field.offset;
    if (avail < field.size) return "<truncated>";

    std::ostringstream oss;

    if (field.isBitfield && field.bitWidth > 0) {
        uint64_t raw = 0;
        size_t bytesToRead = std::min(field.size, sizeof(uint64_t));
        if (bytesToRead > avail) bytesToRead = avail;
        std::memcpy(&raw, ptr, bytesToRead);
        uint64_t shifted = (field.bitOffset < 64) ? (raw >> field.bitOffset) : 0;
        uint64_t mask = (field.bitWidth >= 64) ? ~0ULL : ((1ULL << field.bitWidth) - 1);
        uint64_t val = shifted & mask;
        oss << "0x" << std::hex << val << " (" << std::dec << val << ")";
        return oss.str();
    }

    if (field.isPointer) {
        uint64_t addr = readValAt<uint64_t>(ptr);
        oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << addr;
        if (addr == 0) oss << " (NULL)";
        return oss.str();
    }

    if (field.kind == FieldKind::String || (field.arrayCount > 1 && field.kind == FieldKind::Int8)) {
        // String / char array
        size_t len = 0;
        while (len < field.size && ptr[len] != '\0') ++len;
        std::string s(reinterpret_cast<const char*>(ptr), len);
        oss << "\"" << s << "\"";
        return oss.str();
    }

    if (field.arrayCount > 1) {
        oss << "[";
        size_t elemSize = field.size / field.arrayCount;
        for (size_t i = 0; i < field.arrayCount && i < 8; ++i) {
            if (i > 0) oss << ", ";
            const uint8_t* elemPtr = ptr + i * elemSize;
            switch (field.kind) {
                case FieldKind::Int8: oss << static_cast<int>(*reinterpret_cast<const int8_t*>(elemPtr)); break;
                case FieldKind::UInt8: oss << static_cast<int>(*elemPtr); break;
                case FieldKind::Int16: oss << readValAt<int16_t>(elemPtr); break;
                case FieldKind::UInt16: oss << readValAt<uint16_t>(elemPtr); break;
                case FieldKind::Int32: oss << readValAt<int32_t>(elemPtr); break;
                case FieldKind::UInt32: oss << readValAt<uint32_t>(elemPtr); break;
                case FieldKind::Int64: oss << readValAt<int64_t>(elemPtr); break;
                case FieldKind::UInt64: oss << readValAt<uint64_t>(elemPtr); break;
                case FieldKind::Float: oss << readValAt<float>(elemPtr); break;
                case FieldKind::Double: oss << readValAt<double>(elemPtr); break;
                default: oss << "0x" << std::hex << static_cast<int>(*elemPtr); break;
            }
        }
        if (field.arrayCount > 8) oss << ", ...";
        oss << "]";
        return oss.str();
    }

    switch (field.kind) {
        case FieldKind::Int8: {
            int8_t v = readValAt<int8_t>(ptr);
            oss << static_cast<int>(v);
            if (std::isprint(static_cast<unsigned char>(v))) {
                oss << " ('" << static_cast<char>(v) << "')";
            }
            break;
        }
        case FieldKind::UInt8: {
            uint8_t v = readValAt<uint8_t>(ptr);
            oss << static_cast<int>(v) << " (0x" << std::hex << static_cast<int>(v) << ")";
            break;
        }
        case FieldKind::Int16: {
            int16_t v = readValAt<int16_t>(ptr);
            oss << v << " (0x" << std::hex << static_cast<uint16_t>(v) << ")";
            break;
        }
        case FieldKind::UInt16: {
            uint16_t v = readValAt<uint16_t>(ptr);
            oss << v << " (0x" << std::hex << v << ")";
            break;
        }
        case FieldKind::Int32: {
            int32_t v = readValAt<int32_t>(ptr);
            oss << v << " (0x" << std::hex << static_cast<uint32_t>(v) << ")";
            break;
        }
        case FieldKind::UInt32: {
            uint32_t v = readValAt<uint32_t>(ptr);
            oss << v << " (0x" << std::hex << v << ")";
            break;
        }
        case FieldKind::Int64: {
            int64_t v = readValAt<int64_t>(ptr);
            oss << v << " (0x" << std::hex << static_cast<uint64_t>(v) << ")";
            break;
        }
        case FieldKind::UInt64: {
            uint64_t v = readValAt<uint64_t>(ptr);
            oss << v << " (0x" << std::hex << v << ")";
            break;
        }
        case FieldKind::Float: {
            float v = readValAt<float>(ptr);
            oss << std::fixed << std::setprecision(4) << v;
            break;
        }
        case FieldKind::Double: {
            double v = readValAt<double>(ptr);
            oss << std::fixed << std::setprecision(6) << v;
            break;
        }
        case FieldKind::Pointer: {
            uint64_t addr = readValAt<uint64_t>(ptr);
            oss << "0x" << std::hex << std::setw(16) << std::setfill('0') << addr;
            break;
        }
        default:
            oss << "0x";
            for (size_t i = 0; i < field.size && i < 8; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ptr[i]);
            }
            break;
    }

    return oss.str();
}

TypeManager::TypeManager() {
    registerDefaultTypes();
}

bool TypeManager::registerStruct(const StructDefinition& def) {
    if (def.name.empty()) return false;
    structs_[def.name] = def;
    return true;
}

const StructDefinition* TypeManager::findStruct(const std::string& name) const {
    auto it = structs_.find(name);
    if (it != structs_.end()) return &it->second;
    return nullptr;
}

std::vector<std::string> TypeManager::structNames() const {
    std::vector<std::string> names;
    names.reserve(structs_.size());
    for (const auto& [n, _] : structs_) {
        names.push_back(n);
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool TypeManager::removeStruct(const std::string& name) {
    return structs_.erase(name) > 0;
}

std::optional<StructDefinition> TypeManager::parseCStruct(const std::string& cCode, std::string* errorMsg) {
    if (ClangAstParser::isAvailable()) {
        auto astDef = ClangAstParser::parseCStruct(cCode, errorMsg);
        if (astDef.has_value()) {
            return astDef;
        }
    }

    std::string cleaned = cleanComments(cCode);
    std::string normalized;
    for (char ch : cleaned) {
        if (ch == '{') {
            normalized += " {\n";
        } else if (ch == ';') {
            normalized += ";\n";
        } else {
            normalized += ch;
        }
    }
    std::istringstream iss(normalized);
    std::string line;

    std::string structName = "UnnamedStruct";
    std::vector<StructField> fields;
    bool inBody = false;

    while (std::getline(iss, line)) {
        stripComments(line);
        std::string s = trim(line);
        if (s.empty()) continue;

        if (!inBody) {
            // Find "struct <Name>"
            auto pos = s.find("struct");
            if (pos != std::string::npos) {
                std::string after = trim(s.substr(pos + 6));
                auto bracePos = after.find('{');
                if (bracePos != std::string::npos) {
                    structName = trim(after.substr(0, bracePos));
                    inBody = true;
                } else {
                    auto spacePos = after.find_first_of(" \t");
                    if (spacePos != std::string::npos) {
                        structName = trim(after.substr(0, spacePos));
                    } else if (!after.empty()) {
                        structName = after;
                    }
                }
            }
            if (s.find('{') != std::string::npos) {
                inBody = true;
            }
            continue;
        }

        // Inside body
        if (s.find('}') != std::string::npos) {
            // End of struct
            auto afterBrace = trim(s.substr(s.find('}') + 1));
            if (!afterBrace.empty() && afterBrace != ";") {
                auto semiPos = afterBrace.find(';');
                std::string altName = trim(afterBrace.substr(0, semiPos));
                if (!altName.empty()) structName = altName;
            }
            break;
        }

        // Field declaration: e.g. "int id;", "char name[32];", "void* pNext;"
        if (s.back() == ';') s.pop_back();
        s = trim(s);
        if (s.empty()) continue;

        // Parse array subscript
        size_t arrayCount = 1;
        auto openBracket = s.find('[');
        auto closeBracket = s.find(']');
        if (openBracket != std::string::npos && closeBracket != std::string::npos && closeBracket > openBracket) {
            std::string countStr = s.substr(openBracket + 1, closeBracket - openBracket - 1);
            try {
                arrayCount = std::stoul(countStr);
            } catch (...) {
                arrayCount = 1;
            }
            s = s.substr(0, openBracket) + s.substr(closeBracket + 1);
            s = trim(s);
        }

        // Tokenize remaining into Type and Name
        auto lastSpace = s.find_last_of(" \t*");
        if (lastSpace == std::string::npos) continue;

        bool isPtr = (s.find('*') != std::string::npos);
        std::string rawType = trim(s.substr(0, lastSpace + 1));
        std::string rawName = trim(s.substr(lastSpace + 1));

        if (rawName.empty() && isPtr) {
            // e.g. "void *ptr" where lastSpace was '*'
            auto starPos = s.find_last_of('*');
            rawType = trim(s.substr(0, starPos + 1));
            rawName = trim(s.substr(starPos + 1));
        }

        // Clean up asterisks from name
        while (!rawName.empty() && rawName.front() == '*') {
            isPtr = true;
            rawName = trim(rawName.substr(1));
        }

        if (rawName.empty() || rawType.empty()) continue;

        StructField field;
        field.name = rawName;
        field.arrayCount = arrayCount;
        field.isPointer = isPtr;
        field.typeName = rawType + (isPtr && rawType.find('*') == std::string::npos ? "*" : "");
        if (arrayCount > 1) {
            field.typeName += "[" + std::to_string(arrayCount) + "]";
        }

        // Determine size and alignment
        if (isPtr) {
            field.kind = FieldKind::Pointer;
            field.size = 8 * arrayCount;
            field.alignment = 8;
        } else {
            std::string t = rawType;
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            if (t == "char" || t == "int8_t" || t == "uint8_t" || t == "unsigned char" || t == "bool" || t == "int8" || t == "u8") {
                field.kind = (t.find("u") != std::string::npos) ? FieldKind::UInt8 : FieldKind::Int8;
                field.size = 1 * arrayCount;
                field.alignment = 1;
            } else if (t == "short" || t == "int16_t" || t == "uint16_t" || t == "unsigned short" || t == "int16" || t == "u16") {
                field.kind = (t.find("u") != std::string::npos) ? FieldKind::UInt16 : FieldKind::Int16;
                field.size = 2 * arrayCount;
                field.alignment = 2;
            } else if (t == "int" || t == "int32_t" || t == "uint32_t" || t == "unsigned int" || t == "int32" || t == "u32") {
                field.kind = (t.find("u") != std::string::npos) ? FieldKind::UInt32 : FieldKind::Int32;
                field.size = 4 * arrayCount;
                field.alignment = 4;
            } else if (t == "long long" || t == "int64_t" || t == "uint64_t" || t == "unsigned long long" || t == "long" || t == "unsigned long" || t == "size_t" || t == "uintptr_t" || t == "int64" || t == "u64") {
                field.kind = (t.find("u") != std::string::npos || t == "size_t" || t == "uintptr_t") ? FieldKind::UInt64 : FieldKind::Int64;
                field.size = 8 * arrayCount;
                field.alignment = 8;
            } else if (t == "float") {
                field.kind = FieldKind::Float;
                field.size = 4 * arrayCount;
                field.alignment = 4;
            } else if (t == "double") {
                field.kind = FieldKind::Double;
                field.size = 8 * arrayCount;
                field.alignment = 8;
            } else {
                // Fallback default
                field.kind = FieldKind::Int32;
                field.size = 4 * arrayCount;
                field.alignment = 4;
            }
        }

        fields.push_back(field);
    }

    if (fields.empty()) {
        if (errorMsg) *errorMsg = "No valid fields found in struct definition.";
        return std::nullopt;
    }

    // Natural alignment layout calculation
    size_t curOffset = 0;
    size_t structAlignment = 1;

    for (auto& f : fields) {
        size_t align = f.alignment;
        if (align == 0) align = 1;
        structAlignment = std::max(structAlignment, align);

        // Align offset
        size_t padding = (align - (curOffset % align)) % align;
        curOffset += padding;
        f.offset = curOffset;
        curOffset += f.size;
    }

    // Final tail padding
    size_t tailPadding = (structAlignment - (curOffset % structAlignment)) % structAlignment;
    curOffset += tailPadding;

    StructDefinition def;
    def.name = structName;
    def.fields = fields;
    def.totalSize = curOffset;
    def.alignment = structAlignment;

    return def;
}

bool TypeManager::parseAndRegister(const std::string& cCode, std::string* errorMsg) {
    auto res = parseCStruct(cCode, errorMsg);
    if (!res.has_value()) return false;
    return registerStruct(*res);
}

std::optional<EvaluatedStruct> TypeManager::evaluate(
    const std::string& structName,
    Address baseAddr,
    IDebugBackend& engine) const {

    const auto* def = findStruct(structName);
    if (!def || def->totalSize == 0 || !engine.isAttached()) return std::nullopt;

    std::vector<uint8_t> buffer(def->totalSize);
    if (!engine.readMemory(baseAddr, buffer.data(), def->totalSize)) {
        return std::nullopt;
    }

    EvaluatedStruct evaluated;
    evaluated.structName = def->name;
    evaluated.baseAddress = baseAddr;
    evaluated.totalSize = def->totalSize;

    for (const auto& f : def->fields) {
        EvaluatedField ef;
        ef.offset = f.offset;
        ef.name = f.name;
        ef.typeName = f.typeName;
        ef.size = f.size;
        ef.bitOffset = f.bitOffset;
        ef.bitWidth = f.bitWidth;
        ef.isBitfield = f.isBitfield;

        if (f.offset + f.size <= buffer.size()) {
            ef.rawBytes.assign(buffer.begin() + f.offset, buffer.begin() + f.offset + f.size);
            ef.formattedValue = def->formatFieldValue(f, buffer.data(), buffer.size());
            if (f.isPointer && f.size >= 8) {
                ef.pointerTarget = readValAt<uint64_t>(buffer.data() + f.offset);
            }
        }
        evaluated.fields.push_back(std::move(ef));
    }

    return evaluated;
}

void TypeManager::registerDefaultTypes() {
    // 1. Linux kernel list_head
    parseAndRegister(R"(
        struct list_head {
            void* next;
            void* prev;
        };
    )");

    // 2. POSIX timespec
    parseAndRegister(R"(
        struct timespec {
            long tv_sec;
            long tv_nsec;
        };
    )");

    // 3. POSIX timeval
    parseAndRegister(R"(
        struct timeval {
            long tv_sec;
            long tv_usec;
        };
    )");

    // 4. sockaddr_in
    parseAndRegister(R"(
        struct sockaddr_in {
            short sin_family;
            unsigned short sin_port;
            unsigned int sin_addr;
            char sin_zero[8];
        };
    )");

    // 5. io_vec
    parseAndRegister(R"(
        struct io_vec {
            void* iov_base;
            size_t iov_len;
        };
    )");
}

} // namespace edb_next
