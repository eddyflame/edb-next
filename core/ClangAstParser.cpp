#include "ClangAstParser.hpp"
#include <dlfcn.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

namespace edb_next {

namespace {

// Standard libclang C ABI types
typedef void* CXIndex;
typedef void* CXTranslationUnit;
typedef void* CXClientData;

struct CXString {
    const void* data;
    unsigned private_flags;
};

struct CXCursor {
    int kind;
    int xdata;
    const void* data[3];
};

struct CXType {
    int kind;
    void* data[2];
};

struct CXUnsavedFile {
    const char* Filename;
    const char* Contents;
    unsigned long Length;
};

enum CXCursorKind {
    CXCursor_StructDecl = 2,
    CXCursor_UnionDecl = 3,
    CXCursor_ClassDecl = 4,
    CXCursor_FieldDecl = 6,
    CXCursor_TypedefDecl = 20
};

enum CXTypeKind {
    CXType_Invalid = 0,
    CXType_Void = 2,
    CXType_Bool = 3,
    CXType_Char_U = 4,
    CXType_UChar = 5,
    CXType_Char16 = 6,
    CXType_Char32 = 7,
    CXType_UShort = 8,
    CXType_UInt = 9,
    CXType_ULong = 10,
    CXType_ULongLong = 11,
    CXType_Int128 = 12,
    CXType_Char_S = 13,
    CXType_SChar = 14,
    CXType_WChar = 15,
    CXType_Short = 16,
    CXType_Int = 17,
    CXType_Long = 18,
    CXType_LongLong = 19,
    CXType_Float = 21,
    CXType_Double = 22,
    CXType_LongDouble = 23,
    CXType_Pointer = 101,
    CXType_Record = 105,
    CXType_Enum = 106,
    CXType_Typedef = 107,
    CXType_ConstantArray = 112,
    CXType_Elaborated = 119
};

enum CXChildVisitResult {
    CXChildVisit_Break = 0,
    CXChildVisit_Continue = 1,
    CXChildVisit_Recurse = 2
};

typedef CXChildVisitResult (*CXCursorVisitor)(CXCursor cursor, CXCursor parent, CXClientData client_data);

struct LibClangSymbols {
    void* handle{nullptr};
    CXIndex (*createIndex)(int, int){nullptr};
    void (*disposeIndex)(CXIndex){nullptr};
    CXTranslationUnit (*parseTranslationUnit)(CXIndex, const char*, const char* const*, int, CXUnsavedFile*, unsigned, unsigned){nullptr};
    void (*disposeTranslationUnit)(CXTranslationUnit){nullptr};
    CXCursor (*getTranslationUnitCursor)(CXTranslationUnit){nullptr};
    unsigned (*visitChildren)(CXCursor, CXCursorVisitor, CXClientData){nullptr};
    CXString (*getCursorSpelling)(CXCursor){nullptr};
    CXType (*getCursorType)(CXCursor){nullptr};
    CXString (*getTypeSpelling)(CXType){nullptr};
    long long (*Type_getSizeOf)(CXType){nullptr};
    long long (*Type_getAlignOf)(CXType){nullptr};
    int (*getFieldDeclBitWidth)(CXCursor){nullptr};
    long long (*Cursor_getOffsetOfField)(CXCursor){nullptr};
    const char* (*getCString)(CXString){nullptr};
    void (*disposeString)(CXString){nullptr};
    CXType (*getPointeeType)(CXType){nullptr};
    CXType (*getArrayElementType)(CXType){nullptr};
    long long (*getArraySize)(CXType){nullptr};
    CXType (*getCanonicalType)(CXType){nullptr};
    unsigned (*Cursor_isAnonymous)(CXCursor){nullptr};

    static LibClangSymbols& instance() {
        static LibClangSymbols inst;
        return inst;
    }

    bool load() {
        if (handle) return true;

        const char* candidates[] = {
            "/usr/lib/llvm-18/lib/libclang-18.so.1",
            "/usr/lib/x86_64-linux-gnu/libclang-18.so.1",
            "/usr/lib/llvm-17/lib/libclang-17.so.1",
            "/usr/lib/llvm-16/lib/libclang-16.so.1",
            "/usr/lib/llvm-15/lib/libclang-15.so.1",
            "libclang-18.so.1",
            "libclang.so.1",
            "libclang.so"
        };

        for (const char* path : candidates) {
            handle = ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (handle) break;
        }

        if (!handle) return false;

        #define BIND_SYM(name) name = reinterpret_cast<decltype(name)>(::dlsym(handle, "clang_" #name)); \
            if (!name) { ::dlclose(handle); handle = nullptr; return false; }

        BIND_SYM(createIndex);
        BIND_SYM(disposeIndex);
        BIND_SYM(parseTranslationUnit);
        BIND_SYM(disposeTranslationUnit);
        BIND_SYM(getTranslationUnitCursor);
        BIND_SYM(visitChildren);
        BIND_SYM(getCursorSpelling);
        BIND_SYM(getCursorType);
        BIND_SYM(getTypeSpelling);
        BIND_SYM(Type_getSizeOf);
        BIND_SYM(Type_getAlignOf);
        BIND_SYM(getFieldDeclBitWidth);
        BIND_SYM(Cursor_getOffsetOfField);
        BIND_SYM(getCString);
        BIND_SYM(disposeString);
        BIND_SYM(getPointeeType);
        BIND_SYM(getArrayElementType);
        BIND_SYM(getArraySize);
        BIND_SYM(getCanonicalType);

        // Optional symbol
        Cursor_isAnonymous = reinterpret_cast<decltype(Cursor_isAnonymous)>(::dlsym(handle, "clang_Cursor_isAnonymous"));

        #undef BIND_SYM
        return true;
    }
};

std::string cxToStr(LibClangSymbols& syms, CXString s) {
    if (!syms.getCString) return "";
    const char* c = syms.getCString(s);
    std::string str = c ? c : "";
    syms.disposeString(s);
    return str;
}

FieldKind mapTypeKind(LibClangSymbols& syms, CXType type, bool& isPointer, size_t& arrayCount) {
    isPointer = false;
    arrayCount = 1;

    CXType canonical = syms.getCanonicalType(type);

    if (canonical.kind == CXType_Pointer) {
        isPointer = true;
        return FieldKind::Pointer;
    }

    if (canonical.kind == CXType_ConstantArray) {
        long long arrSize = syms.getArraySize(canonical);
        arrayCount = (arrSize > 0) ? static_cast<size_t>(arrSize) : 1;
        CXType elemType = syms.getArrayElementType(canonical);
        CXType elemCanon = syms.getCanonicalType(elemType);
        if (elemCanon.kind == CXType_Char_S || elemCanon.kind == CXType_Char_U || elemCanon.kind == CXType_SChar) {
            return FieldKind::Int8;
        }
        if (elemCanon.kind == CXType_UChar) {
            return FieldKind::UInt8;
        }
        bool dummyPtr = false;
        size_t dummyCount = 1;
        return mapTypeKind(syms, elemType, dummyPtr, dummyCount);
    }

    switch (canonical.kind) {
        case CXType_Bool:
        case CXType_Char_U:
        case CXType_UChar:
            return FieldKind::UInt8;
        case CXType_Char_S:
        case CXType_SChar:
            return FieldKind::Int8;
        case CXType_UShort:
            return FieldKind::UInt16;
        case CXType_Short:
            return FieldKind::Int16;
        case CXType_UInt:
            return FieldKind::UInt32;
        case CXType_Int:
            return FieldKind::Int32;
        case CXType_ULong:
        case CXType_ULongLong:
            return FieldKind::UInt64;
        case CXType_Long:
        case CXType_LongLong:
            return FieldKind::Int64;
        case CXType_Float:
            return FieldKind::Float;
        case CXType_Double:
        case CXType_LongDouble:
            return FieldKind::Double;
        case CXType_Record:
            return FieldKind::CustomStruct;
        default:
            return FieldKind::Int32;
    }
}

} // anonymous namespace

bool ClangAstParser::isAvailable() {
    return LibClangSymbols::instance().load();
}

std::optional<StructDefinition> ClangAstParser::parseCStruct(const std::string& cCode, std::string* errorMsg) {
    auto& syms = LibClangSymbols::instance();
    if (!syms.load()) {
        if (errorMsg) *errorMsg = "libclang shared library could not be dynamically loaded";
        return std::nullopt;
    }

    CXIndex idx = syms.createIndex(0, 0);
    if (!idx) {
        if (errorMsg) *errorMsg = "Failed to create clang index";
        return std::nullopt;
    }

    CXUnsavedFile unsaved;
    unsaved.Filename = "input_definition.c";
    unsaved.Contents = cCode.c_str();
    unsaved.Length = cCode.length();

    const char* args[] = {
        "-std=c11",
        "-target", "x86_64-pc-linux-gnu"
    };

    CXTranslationUnit tu = syms.parseTranslationUnit(
        idx,
        "input_definition.c",
        args, 3,
        &unsaved, 1,
        0
    );

    if (!tu) {
        syms.disposeIndex(idx);
        if (errorMsg) *errorMsg = "libclang parseTranslationUnit failed";
        return std::nullopt;
    }

    struct ParseContext {
        LibClangSymbols& syms;
        std::optional<StructDefinition> resultDef;
        std::string foundStructName;
        std::vector<StructField> fields;
        size_t totalSize{0};
        size_t alignment{1};
    } ctx{syms, std::nullopt, "", {}, 0, 1};

    auto topVisitor = [](CXCursor c, CXCursor /*parent*/, CXClientData clientData) -> CXChildVisitResult {
        auto* pCtx = static_cast<ParseContext*>(clientData);
        auto& s = pCtx->syms;

        if (c.kind == CXCursor_StructDecl || c.kind == CXCursor_UnionDecl) {
            std::string name = cxToStr(s, s.getCursorSpelling(c));
            CXType structType = s.getCursorType(c);
            long long size = s.Type_getSizeOf(structType);
            long long align = s.Type_getAlignOf(structType);

            // Found top-level struct/union definition
            pCtx->foundStructName = name.empty() ? "AnonymousStruct" : name;
            pCtx->totalSize = (size > 0) ? static_cast<size_t>(size) : 0;
            pCtx->alignment = (align > 0) ? static_cast<size_t>(align) : 1;

            // Visit fields
            s.visitChildren(c, (CXCursorVisitor)+[](CXCursor fc, CXCursor fp, CXClientData fd) -> CXChildVisitResult {
                auto* ctx = static_cast<ParseContext*>(fd);
                if (fc.kind == CXCursor_FieldDecl) {
                    std::string fieldName = cxToStr(ctx->syms, ctx->syms.getCursorSpelling(fc));
                    CXType fieldType = ctx->syms.getCursorType(fc);
                    std::string typeName = cxToStr(ctx->syms, ctx->syms.getTypeSpelling(fieldType));

                    long long bitOffset = ctx->syms.Cursor_getOffsetOfField(fc);
                    int bitWidth = ctx->syms.getFieldDeclBitWidth(fc);
                    long long fieldSize = ctx->syms.Type_getSizeOf(fieldType);
                    long long fieldAlign = ctx->syms.Type_getAlignOf(fieldType);

                    StructField f;
                    f.name = fieldName.empty() ? ("anon_" + std::to_string(ctx->fields.size())) : fieldName;
                    f.typeName = typeName;
                    f.size = (fieldSize > 0) ? static_cast<size_t>(fieldSize) : 4;
                    f.alignment = (fieldAlign > 0) ? static_cast<size_t>(fieldAlign) : 1;
                    f.kind = mapTypeKind(ctx->syms, fieldType, f.isPointer, f.arrayCount);

                    if (bitWidth > 0) {
                        f.isBitfield = true;
                        f.bitWidth = static_cast<uint32_t>(bitWidth);
                        f.bitOffset = (bitOffset >= 0) ? static_cast<uint32_t>(bitOffset % 8) : 0;
                        f.offset = (bitOffset >= 0) ? static_cast<size_t>(bitOffset / 8) : 0;
                    } else {
                        f.isBitfield = false;
                        f.bitWidth = 0;
                        f.bitOffset = 0;
                        f.offset = (bitOffset >= 0) ? static_cast<size_t>(bitOffset / 8) : 0;
                    }
                    ctx->fields.push_back(std::move(f));
                }
                return CXChildVisit_Continue;
            }, clientData);

            if (!pCtx->fields.empty() || pCtx->totalSize > 0) {
                return CXChildVisit_Break;
            }
        }
        return CXChildVisit_Continue;
    };

    CXCursor tuCursor = syms.getTranslationUnitCursor(tu);
    syms.visitChildren(tuCursor, topVisitor, &ctx);

    syms.disposeTranslationUnit(tu);
    syms.disposeIndex(idx);

    if (ctx.fields.empty() && ctx.totalSize == 0) {
        if (errorMsg) *errorMsg = "No struct declaration with fields found in C snippet";
        return std::nullopt;
    }

    StructDefinition def;
    def.name = ctx.foundStructName;
    def.fields = std::move(ctx.fields);
    def.totalSize = ctx.totalSize;
    def.alignment = ctx.alignment;

    return def;
}

} // namespace edb_next
