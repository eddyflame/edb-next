cmake_minimum_required(VERSION 3.16)
project(edb-next VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization and warnings
add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -g)

# Find Dependencies
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPSTONE REQUIRED capstone)
pkg_check_modules(ZSTD REQUIRED libzstd)

find_library(LIBDW_LIBRARIES NAMES dw libdw.so.1 PATHS /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)
find_library(LIBELF_LIBRARIES NAMES elf libelf.so.1 PATHS /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)

pkg_check_modules(PYTHON REQUIRED python3-embed)
find_library(LUA_LIBRARIES NAMES lua5.4 lua54 liblua5.4.so.0 liblua5.4.so PATHS /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib)

find_path(LIBDW_INCLUDE_DIRS elfutils/libdw.h
    PATHS
    /usr/include
    /usr/local/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdw/include
)

find_path(KEYSTONE_INCLUDE_DIRS keystone/keystone.h
    PATHS
    /usr/include
    /usr/local/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/keystone/include
)

find_library(KEYSTONE_LIBRARIES NAMES keystone libkeystone
    PATHS
    /lib/x86_64-linux-gnu
    /usr/lib/x86_64-linux-gnu
    /usr/lib
    /usr/local/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/keystone/lib
)

if (KEYSTONE_INCLUDE_DIRS AND KEYSTONE_LIBRARIES)
    message(STATUS "Found Keystone In-Memory Assembler: ${KEYSTONE_LIBRARIES}")
    add_compile_definitions(HAVE_KEYSTONE=1)
endif()

find_path(ZYDIS_INCLUDE_DIRS Zydis/Zydis.h
    PATHS
    /usr/include
    /usr/local/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zydis/include
)

find_library(ZYDIS_LIBRARIES NAMES zydis libzydis Zydis libZydis
    PATHS
    /lib/x86_64-linux-gnu
    /usr/lib/x86_64-linux-gnu
    /usr/lib
    /usr/local/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zydis/lib
)

if (ZYDIS_INCLUDE_DIRS AND ZYDIS_LIBRARIES)
    message(STATUS "Found Zydis x86_64 Fast Decoder: ${ZYDIS_LIBRARIES}")
    add_compile_definitions(HAVE_ZYDIS=1)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CAPSTONE_INCLUDE_DIRS}
    ${LIBDW_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdw/include
    ${PYTHON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/include
    ${KEYSTONE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/keystone/include
    ${ZYDIS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zydis/include
    ${ZSTD_INCLUDE_DIRS}
)

# Core Library
set(CORE_SOURCES
    core/BreakpointManager.cpp
    core/PageGuardManager.cpp
    core/RendezvousManager.cpp
    core/LinuxDebugEngine.cpp
    core/EventLoopThread.cpp
    core/ElfParser.cpp
    core/DwarfParser.cpp
    core/SourceFileManager.cpp
    core/CallStackUnwinder.cpp
    core/StringScanner.cpp
    core/AnnotationManager.cpp
    core/FunctionFinder.cpp
    core/HeapAnalyzer.cpp
    core/DebugSession.cpp
    core/SessionManager.cpp
    core/ExpressionEvaluator.cpp
    core/Assembler.cpp
    core/ROPScanner.cpp
    core/InstructionInspector.cpp
    core/CodeXRefFinder.cpp
    core/PatternSearcher.cpp
    core/ConfigurationManager.cpp
    core/PluginManager.cpp
    core/PatchManager.cpp
    core/TraceEngine.cpp
    core/LogManager.cpp
    core/IntermodularCallsFinder.cpp
    core/DatabaseManager.cpp
    core/OpcodeSearcher.cpp
    core/StateDumper.cpp
    core/PythonScriptEngine.cpp
    core/LuaScriptEngine.cpp
    core/ScriptEngineManager.cpp
    core/MemoryScanner.cpp
    core/TypeManager.cpp
    core/CapstoneContext.cpp
    core/ZydisContext.cpp
    core/CFGBuilder.cpp
    core/ScriptApiBridge.cpp
    core/ClangAstParser.cpp
)

add_library(edb_core STATIC ${CORE_SOURCES})
target_link_libraries(edb_core
    PUBLIC
    Qt6::Core
    Qt6::Gui
    ${CAPSTONE_LIBRARIES}
    ${LIBDW_LIBRARIES}
    ${LIBELF_LIBRARIES}
    ${PYTHON_LIBRARIES}
    ${LUA_LIBRARIES}
    ${KEYSTONE_LIBRARIES}
    ${ZYDIS_LIBRARIES}
    ${ZSTD_LIBRARIES}
    ${CMAKE_DL_LIBS}
    pthread
)

# UI & Executable
set(UI_SOURCES
    ui/DisassemblyView.cpp
    ui/SourceView.cpp
    ui/ScriptConsoleView.cpp
    ui/RegisterView.cpp
    ui/MemoryHexView.cpp
    ui/MemoryRegionsView.cpp
    ui/BreakpointManagerView.cpp
    ui/CallStackView.cpp
    ui/StringReferencesView.cpp
    ui/SymbolViewer.cpp
    ui/ProcessPropertiesView.cpp
    ui/HeapView.cpp
    ui/ThreadsView.cpp
    ui/ROPToolView.cpp
    ui/XRefDialog.cpp
    ui/AssembleDialog.cpp
    ui/NavigationBus.cpp
    ui/SessionTabWidget.cpp
    ui/PreferencesDialog.cpp
    ui/LaunchArgumentsDialog.cpp
    ui/PluginManagerDialog.cpp
    ui/PatchManagerDialog.cpp
    ui/CommandRegistry.cpp
    ui/CommandBarView.cpp
    ui/WatchView.cpp
    ui/TraceView.cpp
    ui/CFGGraphView.cpp
    ui/SugiyamaLayout.cpp
    ui/NotesView.cpp
    ui/LogView.cpp
    ui/BinaryInfoView.cpp
    ui/IntermodularCallsView.cpp
    ui/MultiDumpWidget.cpp
    ui/OpcodeSearcherView.cpp
    ui/StackView.cpp
    ui/MemoryScannerView.cpp
    ui/TypeViewer.cpp
    ui/MainWindow.cpp
)

add_executable(edb_next ${UI_SOURCES} main.cpp)
target_link_libraries(edb_next
    PRIVATE
    edb_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    pthread
)

# Sample Plugin Shared Library
add_library(sample_plugin SHARED plugins/SamplePlugin/SamplePlugin.cpp)
set_target_properties(sample_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/plugins
    PREFIX ""
)
target_link_libraries(sample_plugin
    PRIVATE
    edb_core
    Qt6::Widgets
    Qt6::Core
)
add_dependencies(edb_next sample_plugin)

# Test target
add_custom_target(build_test_target
    COMMAND gcc -g -O0 -pthread ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_target.c -o ${CMAKE_CURRENT_BINARY_DIR}/test_target
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling test target binary..."
)
add_dependencies(edb_next build_test_target)

# Unit test executable
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core
    PRIVATE
    edb_core
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    pthread
)
add_dependencies(test_core build_test_target)

# Advanced test suite (Milestones 1 ~ 3: Config, Plugin, Patch, Trace, CFG)
add_executable(test_advanced tests/test_advanced.cpp ${UI_SOURCES})
target_link_libraries(test_advanced
    PRIVATE
    edb_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    pthread
)
add_dependencies(test_advanced build_test_target sample_plugin)

# Exit test executable
add_executable(test_exit tests/test_exit.cpp ${UI_SOURCES})
target_link_libraries(test_exit
    PRIVATE
    edb_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    pthread
)
add_dependencies(test_exit build_test_target)

# DWARF & Source-Level Debugging test executable
add_executable(test_dwarf tests/test_dwarf.cpp ${UI_SOURCES})
target_link_libraries(test_dwarf
    PRIVATE
    edb_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    pthread
)
add_dependencies(test_dwarf build_test_target)

# Scripting Engine test executable (Python 3 & Lua 5.4)
add_executable(test_scripting tests/test_scripting.cpp ${UI_SOURCES})
target_link_libraries(test_scripting
    PRIVATE
    edb_core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    pthread
)
add_dependencies(test_scripting build_test_target)

# Next-Gen (P3) test executable (pidfd, ClangAstParser, SQLite3+zstd, C++23)
add_executable(test_nextgen tests/test_nextgen.cpp)
target_link_libraries(test_nextgen
    PRIVATE
    edb_core
    Qt6::Core
    ${CAPSTONE_LIBRARIES}
    ${ZSTD_LIBRARIES}
    ${CMAKE_DL_LIBS}
    pthread
)
add_dependencies(test_nextgen build_test_target)

# ==============================================================================
# Installation Rules (Standard FHS & AppImage Packaging Support)
# ==============================================================================
include(GNUInstallDirs)

# Main Executable
install(TARGETS edb_next
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Built-in Plugins
install(TARGETS sample_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/edb-next/plugins
)

# Desktop Entry
install(FILES res/edb-next.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Icons (SVG & PNG)
install(FILES res/edb-next.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES res/edb-next.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    OPTIONAL
)

