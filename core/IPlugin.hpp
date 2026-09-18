#pragma once

#include <string>
#include <vector>
#include <QtPlugin>

namespace edb_next {

class IPluginContext;

enum class ContextMenuTarget {
    Disassembly,
    Registers,
    Stack,
    MemoryDump
};

struct PluginMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Metadata
    [[nodiscard]] virtual PluginMetadata metadata() const = 0;

    // Lifecycle
    virtual bool initialize(IPluginContext* context) = 0;
    virtual void shutdown() = 0;
};

} // namespace edb_next

#define EDB_NEXT_PLUGIN_IID "com.edb_next.IPlugin/1.0"
Q_DECLARE_INTERFACE(edb_next::IPlugin, EDB_NEXT_PLUGIN_IID)
