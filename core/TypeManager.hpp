#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace edb_next {

class LinuxDebugEngine;

enum class FieldKind {
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    Pointer,
    String,
    ByteArray,
    CustomStruct
};

struct StructField {
    std::string name;
    std::string typeName;
    FieldKind kind{FieldKind::Int32};
    size_t offset{0};
    size_t size{4};
    size_t alignment{4};
    size_t arrayCount{1};
    bool isPointer{false};
};

struct StructDefinition {
    std::string name;
    std::vector<StructField> fields;
    size_t totalSize{0};
    size_t alignment{1};

    std::string formatFieldValue(const StructField& field, const uint8_t* mem, size_t memLen) const;
};

struct EvaluatedField {
    size_t offset{0};
    std::string name;
    std::string typeName;
    size_t size{0};
    std::vector<uint8_t> rawBytes;
    std::string formattedValue;
    uint64_t pointerTarget{0};
};

struct EvaluatedStruct {
    std::string structName;
    Address baseAddress{0};
    size_t totalSize{0};
    std::vector<EvaluatedField> fields;
};

class TypeManager {
public:
    TypeManager();
    ~TypeManager() = default;

    // Register a pre-constructed struct definition
    bool registerStruct(const StructDefinition& def);

    // Parse standard C struct source code, compute AMD64 layout, and register
    bool parseAndRegister(const std::string& cCode, std::string* errorMsg = nullptr);

    // Query registered struct definitions
    [[nodiscard]] const StructDefinition* findStruct(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> structNames() const;
    bool removeStruct(const std::string& name);

    // Read live memory from target engine and evaluate all fields of the struct
    [[nodiscard]] std::optional<EvaluatedStruct> evaluate(
        const std::string& structName,
        Address baseAddr,
        LinuxDebugEngine& engine) const;

    // Static parser helper for C struct definitions
    static std::optional<StructDefinition> parseCStruct(const std::string& cCode, std::string* errorMsg = nullptr);

    // Built-in standard POSIX / Linux kernel structs
    void registerDefaultTypes();

private:
    std::unordered_map<std::string, StructDefinition> structs_;
};

} // namespace edb_next
