#include "BtfParser.hpp"
#include <linux/btf.h>
#include <fstream>
#include <cstring>
#include <format>
#include <algorithm>
#include <elf.h>
#include <iostream>

// Compatibility definitions for BTF kinds and structures missing in older Linux kernel headers (< Linux 6.0)
#ifndef BTF_KIND_FLOAT
#define BTF_KIND_FLOAT 16
#endif
#ifndef BTF_KIND_DECL_TAG
#define BTF_KIND_DECL_TAG 17
#endif
#ifndef BTF_KIND_TYPE_TAG
#define BTF_KIND_TYPE_TAG 18
#endif
#ifndef BTF_KIND_ENUM64
#define BTF_KIND_ENUM64 19
#endif

namespace {
struct BtfDeclTagCompat {
    int32_t component_idx;
};
} // namespace

namespace edb_next {

BtfParser::BtfParser() = default;

void BtfParser::clear() {
    stringTable_.clear();
    typesById_.clear();
    nameToId_.clear();
}

std::string BtfParser::getString(uint32_t offset) const {
    if (offset >= stringTable_.size()) {
        return {};
    }
    const char* str = reinterpret_cast<const char*>(stringTable_.data() + offset);
    size_t max_len = stringTable_.size() - offset;
    size_t len = ::strnlen(str, max_len);
    return std::string(str, len);
}

bool BtfParser::parseBuffer(std::span<const uint8_t> data) {
    clear();

    if (data.size() < sizeof(struct btf_header)) {
        return false;
    }

    const auto* hdr = reinterpret_cast<const struct btf_header*>(data.data());
    if (hdr->magic != BTF_MAGIC && hdr->magic != 0xeb9f) {
        return false;
    }

    uint64_t base_offset = hdr->hdr_len;
    uint64_t type_start = base_offset + hdr->type_off;
    uint64_t type_end = type_start + hdr->type_len;
    uint64_t str_start = base_offset + hdr->str_off;
    uint64_t str_end = str_start + hdr->str_len;

    if (type_end > data.size() || str_end > data.size()) {
        return false;
    }

    stringTable_.assign(data.data() + str_start, data.data() + str_end);

    const uint8_t* ptr = data.data() + type_start;
    const uint8_t* end = data.data() + type_end;

    uint32_t current_id = 1;

    while (ptr + sizeof(struct btf_type) <= end) {
        const auto* t = reinterpret_cast<const struct btf_type*>(ptr);
        ptr += sizeof(struct btf_type);

        uint8_t kind = static_cast<uint8_t>((t->info >> 24) & 0x1f);
        uint16_t vlen = static_cast<uint16_t>(t->info & 0xffff);
        bool kflag = (t->info & (1U << 31)) != 0;

        BtfTypeEntry entry{};
        entry.id = current_id;
        entry.name = getString(t->name_off);
        entry.kind = kind;
        entry.kindFlag = kflag;

        switch (kind) {
            case BTF_KIND_INT: {
                entry.size = t->size;
                if (ptr + sizeof(uint32_t) <= end) {
                    ptr += sizeof(uint32_t); // Skip btf_int encoding
                }
                break;
            }
            case BTF_KIND_PTR:
            case BTF_KIND_TYPEDEF:
            case BTF_KIND_VOLATILE:
            case BTF_KIND_CONST:
            case BTF_KIND_RESTRICT:
            case BTF_KIND_TYPE_TAG: {
                entry.targetTypeId = t->type;
                break;
            }
            case BTF_KIND_ARRAY: {
                if (ptr + sizeof(struct btf_array) <= end) {
                    const auto* arr = reinterpret_cast<const struct btf_array*>(ptr);
                    entry.targetTypeId = arr->type;
                    entry.size = arr->nelems;
                    ptr += sizeof(struct btf_array);
                }
                break;
            }
            case BTF_KIND_STRUCT:
            case BTF_KIND_UNION: {
                entry.size = t->size;
                size_t members_size = vlen * sizeof(struct btf_member);
                if (ptr + members_size <= end) {
                    const auto* members = reinterpret_cast<const struct btf_member*>(ptr);
                    ptr += members_size;

                    for (uint16_t i = 0; i < vlen; ++i) {
                        const auto& m = members[i];
                        StructField field{};
                        field.name = getString(m.name_off);
                        field.typeName = resolveTypeName(m.type);

                        uint32_t bit_offset = 0;
                        uint32_t bit_width = 0;
                        if (kflag) {
                            bit_offset = m.offset & 0x00ffffff;
                            bit_width = (m.offset >> 24) & 0xff;
                        } else {
                            bit_offset = m.offset;
                            bit_width = 0;
                        }

                        field.offset = bit_offset / 8;
                        field.bitOffset = bit_offset % 8;
                        field.bitWidth = bit_width;
                        field.isBitfield = (bit_width > 0);
                        field.size = (bit_width > 0) ? ((bit_width + 7) / 8) : 4;
                        field.kind = typeIdToFieldKind(m.type);

                        entry.fields.push_back(field);
                    }
                }
                break;
            }
            case BTF_KIND_ENUM: {
                entry.size = t->size;
                size_t enum_size = vlen * sizeof(struct btf_enum);
                if (ptr + enum_size <= end) {
                    ptr += enum_size;
                }
                break;
            }
            case BTF_KIND_ENUM64: {
                entry.size = t->size;
                // btf_enum64 is 12 bytes
                size_t enum64_size = vlen * 12;
                if (ptr + enum64_size <= end) {
                    ptr += enum64_size;
                }
                break;
            }
            case BTF_KIND_FWD:
            case BTF_KIND_FLOAT: {
                entry.size = t->size;
                break;
            }
            case BTF_KIND_FUNC: {
                entry.targetTypeId = t->type;
                break;
            }
            case BTF_KIND_FUNC_PROTO: {
                entry.targetTypeId = t->type;
                size_t proto_size = vlen * sizeof(struct btf_param);
                if (ptr + proto_size <= end) {
                    ptr += proto_size;
                }
                break;
            }
            case BTF_KIND_VAR: {
                entry.targetTypeId = t->type;
                if (ptr + sizeof(struct btf_var) <= end) {
                    ptr += sizeof(struct btf_var);
                }
                break;
            }
            case BTF_KIND_DATASEC: {
                entry.size = t->size;
                size_t datasec_size = vlen * sizeof(struct btf_var_secinfo);
                if (ptr + datasec_size <= end) {
                    ptr += datasec_size;
                }
                break;
            }
            case BTF_KIND_DECL_TAG: {
                entry.targetTypeId = t->type;
                if (ptr + sizeof(BtfDeclTagCompat) <= end) {
                    ptr += sizeof(BtfDeclTagCompat);
                }
                break;
            }
            default:
                break;
        }

        if (!entry.name.empty()) {
            nameToId_[entry.name] = entry.id;
        }
        typesById_[entry.id] = std::move(entry);
        current_id++;
    }

    return !typesById_.empty();
}

bool BtfParser::parseFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0 || size > 128 * 1024 * 1024) { // 128MB sanity limit
        return false;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }

    return parseBuffer(buffer);
}

bool BtfParser::parseElfSection(const std::string& elfPath, const std::string& sectionName) {
    std::ifstream file(elfPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    Elf64_Ehdr ehdr{};
    if (!file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr))) {
        return false;
    }

    if (std::memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 || ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        return false;
    }

    if (ehdr.e_shoff == 0 || ehdr.e_shnum == 0) {
        return false;
    }

    file.seekg(ehdr.e_shoff, std::ios::beg);
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    if (!file.read(reinterpret_cast<char*>(shdrs.data()), ehdr.e_shnum * sizeof(Elf64_Shdr))) {
        return false;
    }

    if (ehdr.e_shstrndx >= ehdr.e_shnum) {
        return false;
    }

    const auto& str_shdr = shdrs[ehdr.e_shstrndx];
    std::vector<char> shstrtab(str_shdr.sh_size);
    file.seekg(str_shdr.sh_offset, std::ios::beg);
    if (!file.read(shstrtab.data(), str_shdr.sh_size)) {
        return false;
    }

    for (const auto& shdr : shdrs) {
        if (shdr.sh_name < shstrtab.size()) {
            std::string name(shstrtab.data() + shdr.sh_name);
            if (name == sectionName && shdr.sh_size > 0) {
                std::vector<uint8_t> btf_blob(shdr.sh_size);
                file.seekg(shdr.sh_offset, std::ios::beg);
                if (file.read(reinterpret_cast<char*>(btf_blob.data()), shdr.sh_size)) {
                    return parseBuffer(btf_blob);
                }
            }
        }
    }

    return false;
}

bool BtfParser::parseVmlinux(const std::string& path) {
    return parseFile(path);
}

const BtfTypeEntry* BtfParser::findType(const std::string& name) const {
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        return getTypeById(it->second);
    }
    return nullptr;
}

const BtfTypeEntry* BtfParser::getTypeById(uint32_t id) const {
    auto it = typesById_.find(id);
    if (it != typesById_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> BtfParser::allTypeNames() const {
    std::vector<std::string> names;
    names.reserve(nameToId_.size());
    for (const auto& [name, _] : nameToId_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> BtfParser::structAndUnionNames() const {
    std::vector<std::string> names;
    for (const auto& [_, entry] : typesById_) {
        if ((entry.kind == BTF_KIND_STRUCT || entry.kind == BTF_KIND_UNION) && !entry.name.empty()) {
            names.push_back(entry.name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string BtfParser::resolveTypeName(uint32_t typeId) const {
    if (typeId == 0) return "void";
    const auto* t = getTypeById(typeId);
    if (!t) return "unknown";
    if (!t->name.empty()) return t->name;

    if (t->kind == BTF_KIND_PTR) {
        return resolveTypeName(t->targetTypeId) + "*";
    }
    if (t->kind == BTF_KIND_TYPEDEF || t->kind == BTF_KIND_CONST || t->kind == BTF_KIND_VOLATILE) {
        return resolveTypeName(t->targetTypeId);
    }
    return std::format("type_{}", typeId);
}

FieldKind BtfParser::typeIdToFieldKind(uint32_t typeId) const {
    if (typeId == 0) return FieldKind::Pointer;
    const auto* t = getTypeById(typeId);
    if (!t) return FieldKind::Int32;

    if (t->kind == BTF_KIND_PTR) {
        return FieldKind::Pointer;
    }
    if (t->kind == BTF_KIND_INT) {
        if (t->size == 1) return FieldKind::Int8;
        if (t->size == 2) return FieldKind::Int16;
        if (t->size == 4) return FieldKind::Int32;
        if (t->size == 8) return FieldKind::Int64;
    }
    if (t->kind == BTF_KIND_FLOAT) {
        if (t->size == 4) return FieldKind::Float;
        if (t->size == 8) return FieldKind::Double;
    }
    if (t->kind == BTF_KIND_STRUCT || t->kind == BTF_KIND_UNION) {
        return FieldKind::CustomStruct;
    }
    if (t->kind == BTF_KIND_TYPEDEF || t->kind == BTF_KIND_CONST || t->kind == BTF_KIND_VOLATILE) {
        return typeIdToFieldKind(t->targetTypeId);
    }
    return FieldKind::Int32;
}

size_t BtfParser::exportToTypeManager(TypeManager& typeMgr) const {
    size_t count = 0;
    for (const auto& [_, entry] : typesById_) {
        if ((entry.kind == BTF_KIND_STRUCT || entry.kind == BTF_KIND_UNION) && !entry.name.empty()) {
            StructDefinition def{};
            def.name = entry.name;
            def.totalSize = entry.size;
            def.alignment = (entry.size >= 8) ? 8 : ((entry.size >= 4) ? 4 : 1);
            def.fields = entry.fields;
            if (typeMgr.registerStruct(def)) {
                count++;
            }
        }
    }
    return count;
}

std::vector<uint8_t> BtfParser::createSyntheticBtf(
    const std::string& structName,
    const std::vector<std::pair<std::string, uint32_t>>& fieldsWithSizes) {

    // Construct minimal BTF with:
    // String table: "\0" + structName + fieldNames...
    // Type 1: INT (int32_t, size 4)
    // Type 2: STRUCT (structName, members...)

    std::vector<uint8_t> strtab = {0}; // initial null byte

    auto add_str = [&](const std::string& s) -> uint32_t {
        uint32_t off = static_cast<uint32_t>(strtab.size());
        for (char c : s) strtab.push_back(static_cast<uint8_t>(c));
        strtab.push_back(0);
        return off;
    };

    uint32_t sname_off = add_str(structName);
    uint32_t int_name_off = add_str("int");

    std::vector<uint32_t> fname_offsets;
    for (const auto& [fname, _] : fieldsWithSizes) {
        fname_offsets.push_back(add_str(fname));
    }

    std::vector<uint8_t> types_data;

    // Type 1: INT (size 4)
    struct btf_type int_t{};
    int_t.name_off = int_name_off;
    int_t.info = (BTF_KIND_INT << 24);
    int_t.size = 4;
    const auto* p_int = reinterpret_cast<const uint8_t*>(&int_t);
    types_data.insert(types_data.end(), p_int, p_int + sizeof(int_t));
    uint32_t int_enc = 0x01000020; // 32 bits, signed
    const auto* p_enc = reinterpret_cast<const uint8_t*>(&int_enc);
    types_data.insert(types_data.end(), p_enc, p_enc + sizeof(int_enc));

    // Type 2: STRUCT
    struct btf_type st_t{};
    st_t.name_off = sname_off;
    uint16_t vlen = static_cast<uint16_t>(fieldsWithSizes.size());
    st_t.info = (BTF_KIND_STRUCT << 24) | vlen;

    uint32_t current_offset = 0;
    std::vector<struct btf_member> members;
    for (size_t i = 0; i < fieldsWithSizes.size(); ++i) {
        struct btf_member m{};
        m.name_off = fname_offsets[i];
        m.type = 1; // Type 1: int
        m.offset = current_offset * 8; // bit offset
        current_offset += fieldsWithSizes[i].second;
        members.push_back(m);
    }
    st_t.size = current_offset;

    const auto* p_st = reinterpret_cast<const uint8_t*>(&st_t);
    types_data.insert(types_data.end(), p_st, p_st + sizeof(st_t));

    for (const auto& m : members) {
        const auto* p_m = reinterpret_cast<const uint8_t*>(&m);
        types_data.insert(types_data.end(), p_m, p_m + sizeof(m));
    }

    struct btf_header hdr{};
    hdr.magic = BTF_MAGIC;
    hdr.version = BTF_VERSION;
    hdr.flags = 0;
    hdr.hdr_len = sizeof(struct btf_header);
    hdr.type_off = 0;
    hdr.type_len = static_cast<uint32_t>(types_data.size());
    hdr.str_off = hdr.type_len;
    hdr.str_len = static_cast<uint32_t>(strtab.size());

    std::vector<uint8_t> result;
    const auto* p_hdr = reinterpret_cast<const uint8_t*>(&hdr);
    result.insert(result.end(), p_hdr, p_hdr + sizeof(hdr));
    result.insert(result.end(), types_data.begin(), types_data.end());
    result.insert(result.end(), strtab.begin(), strtab.end());

    return result;
}

} // namespace edb_next
