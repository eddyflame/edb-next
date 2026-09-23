#include "DapServer.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace edb_next {

namespace {

std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";

    size_t startQuote = json.find('\"', pos);
    if (startQuote == std::string::npos) return "";

    size_t endQuote = json.find('\"', startQuote + 1);
    if (endQuote == std::string::npos) return "";

    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

int extractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defaultVal;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;

    size_t numStart = json.find_first_of("0123456789-", pos);
    if (numStart == std::string::npos) return defaultVal;

    size_t numEnd = json.find_first_not_of("0123456789-", numStart);
    std::string valStr = json.substr(numStart, (numEnd == std::string::npos ? json.size() : numEnd) - numStart);
    try {
        return std::stoi(valStr);
    } catch (...) {
        return defaultVal;
    }
}

uint64_t parseAddress(const std::string& str) {
    if (str.empty()) return 0;
    try {
        if (str.starts_with("0x") || str.starts_with("0X")) {
            return std::stoull(str, nullptr, 16);
        }
        return std::stoull(str, nullptr, 10);
    } catch (...) {
        return 0;
    }
}

std::string formatDapResponse(int seq, int requestSeq, const std::string& command,
                              bool success, const std::string& bodyJson = "{}") {
    std::ostringstream oss;
    oss << "{"
        << "\"seq\":" << seq << ","
        << "\"type\":\"response\","
        << "\"request_seq\":" << requestSeq << ","
        << "\"success\":" << (success ? "true" : "false") << ","
        << "\"command\":\"" << command << "\","
        << "\"body\":" << bodyJson
        << "}";
    return oss.str();
}

std::string frameDapMessage(const std::string& json) {
    std::ostringstream oss;
    oss << "Content-Length: " << json.size() << "\r\n\r\n" << json;
    return oss.str();
}

} // anonymous namespace

struct DapServer::Impl {
    int seqCounter{1};
};

DapServer::DapServer()
    : pImpl_(std::make_unique<Impl>()),
      engine_(std::make_shared<LinuxDebugEngine>()) {}

DapServer::DapServer(std::shared_ptr<LinuxDebugEngine> engine)
    : pImpl_(std::make_unique<Impl>()),
      engine_(std::move(engine)) {}

DapServer::~DapServer() = default;
DapServer::DapServer(DapServer&&) noexcept = default;
DapServer& DapServer::operator=(DapServer&&) noexcept = default;

void DapServer::setEngine(std::shared_ptr<LinuxDebugEngine> engine) {
    engine_ = std::move(engine);
}

std::string DapServer::handleMessage(const std::string& rawMessage) {
    std::string json = rawMessage;
    // Strip HTTP-like header if present
    size_t headerEnd = json.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
        json = json.substr(headerEnd + 4);
    }

    std::string command = extractJsonString(json, "command");
    int reqSeq = extractJsonInt(json, "seq", 1);
    int resSeq = pImpl_->seqCounter++;

    if (command == "initialize") {
        std::string body = "{"
            "\"supportsConfigurationDoneRequest\":true,"
            "\"supportsFunctionBreakpoints\":true,"
            "\"supportsConditionalBreakpoints\":true,"
            "\"supportsEvaluateForHovers\":true,"
            "\"supportsStepBack\":true,"
            "\"supportsSetVariable\":true,"
            "\"supportsDisassembleRequest\":true,"
            "\"supportsReadMemoryRequest\":true,"
            "\"supportsWriteMemoryRequest\":true"
        "}";
        return formatDapResponse(resSeq, reqSeq, command, true, body);
    }

    if (command == "launch") {
        std::string prog = extractJsonString(json, "program");
        bool ok = true;
        if (!prog.empty() && engine_) {
            ok = engine_->launch(prog, {}).success;
        }
        return formatDapResponse(resSeq, reqSeq, command, ok, "{}");
    }

    if (command == "attach") {
        int pid = extractJsonInt(json, "pid", 0);
        bool ok = false;
        if (pid > 0 && engine_) {
            ok = engine_->attach(pid).success;
        }
        return formatDapResponse(resSeq, reqSeq, command, ok, "{}");
    }

    if (command == "setBreakpoints") {
        std::string body = "{\"breakpoints\":[{\"verified\":true,\"line\":1}]}";
        return formatDapResponse(resSeq, reqSeq, command, true, body);
    }

    if (command == "configurationDone") {
        return formatDapResponse(resSeq, reqSeq, command, true, "{}");
    }

    if (command == "threads") {
        int tid = (engine_ && engine_->activeTid() > 0) ? engine_->activeTid() : 1;
        std::ostringstream oss;
        oss << "{\"threads\":[{\"id\":" << tid << ",\"name\":\"Main Thread (" << tid << ")\"}]}";
        return formatDapResponse(resSeq, reqSeq, command, true, oss.str());
    }

    if (command == "stackTrace") {
        uint64_t ripVal = 0x401000;
        if (engine_ && engine_->activeTid() > 0) {
            RegisterContext regs;
            if (engine_->getRegisters(engine_->activeTid(), regs)) {
                ripVal = regs.rip().value();
            }
        }
        std::ostringstream oss;
        oss << "{\"stackFrames\":[{"
            << "\"id\":1,"
            << "\"name\":\"main\","
            << "\"line\":1,"
            << "\"column\":1,"
            << "\"instructionPointerReference\":\"0x" << std::hex << ripVal << "\""
            << "}],\"totalFrames\":1}";
        return formatDapResponse(resSeq, reqSeq, command, true, oss.str());
    }

    if (command == "scopes") {
        std::string body = "{\"scopes\":["
            "{\"name\":\"Registers\",\"variablesReference\":1,\"expensive\":false},"
            "{\"name\":\"Locals\",\"variablesReference\":2,\"expensive\":false}"
        "]}";
        return formatDapResponse(resSeq, reqSeq, command, true, body);
    }

    if (command == "variables") {
        int varRef = extractJsonInt(json, "variablesReference", 0);
        if (varRef == 1) {
            RegisterContext regs;
            if (engine_ && engine_->activeTid() > 0) {
                engine_->getRegisters(engine_->activeTid(), regs);
            }
            std::ostringstream oss;
            oss << "{\"variables\":["
                << "{\"name\":\"rax\",\"value\":\"0x" << std::hex << regs.rax() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rbx\",\"value\":\"0x" << std::hex << regs.rbx() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rcx\",\"value\":\"0x" << std::hex << regs.rcx() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rdx\",\"value\":\"0x" << std::hex << regs.rdx() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rsi\",\"value\":\"0x" << std::hex << regs.rsi() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rdi\",\"value\":\"0x" << std::hex << regs.rdi() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rbp\",\"value\":\"0x" << std::hex << regs.rbp().value() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rsp\",\"value\":\"0x" << std::hex << regs.rsp().value() << "\",\"variablesReference\":0},"
                << "{\"name\":\"rip\",\"value\":\"0x" << std::hex << regs.rip().value() << "\",\"variablesReference\":0}"
                << "]}";
            return formatDapResponse(resSeq, reqSeq, command, true, oss.str());
        }
        return formatDapResponse(resSeq, reqSeq, command, true, "{\"variables\":[]}");
    }

    if (command == "continue") {
        if (engine_ && engine_->activeTid() > 0) {
            engine_->continueExecution(engine_->activeTid(), 0);
        }
        return formatDapResponse(resSeq, reqSeq, command, true, "{}");
    }

    if (command == "next" || command == "stepIn") {
        if (engine_ && engine_->activeTid() > 0) {
            engine_->singleStep(engine_->activeTid(), 0);
        }
        return formatDapResponse(resSeq, reqSeq, command, true, "{}");
    }

    if (command == "stepBack") {
        timeTravel_.stepBack();
        return formatDapResponse(resSeq, reqSeq, command, true, "{}");
    }

    if (command == "pause") {
        if (engine_ && engine_->activeTid() > 0) {
            engine_->pause(engine_->activeTid());
        }
        return formatDapResponse(resSeq, reqSeq, command, true, "{}");
    }

    if (command == "readMemory") {
        std::string memRef = extractJsonString(json, "memoryReference");
        int count = extractJsonInt(json, "count", 16);
        uint64_t addr = parseAddress(memRef);

        std::vector<uint8_t> buf(count, 0);
        if (engine_ && addr > 0) {
            engine_->readMemory(Address(addr), buf.data(), count);
        }

        std::ostringstream hexStream;
        for (uint8_t b : buf) {
            hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }

        std::ostringstream oss;
        oss << "{\"address\":\"0x" << std::hex << addr << "\",\"unreadableBytes\":0,\"data\":\"" << hexStream.str() << "\"}";
        return formatDapResponse(resSeq, reqSeq, command, true, oss.str());
    }

    if (command == "disassemble") {
        std::string memRef = extractJsonString(json, "memoryReference");
        int insnCount = extractJsonInt(json, "instructionCount", 4);
        uint64_t addr = parseAddress(memRef);

        std::ostringstream oss;
        oss << "{\"instructions\":[";
        for (int i = 0; i < insnCount; ++i) {
            uint64_t curAddr = addr + i * 4;
            oss << "{\"address\":\"0x" << std::hex << curAddr << "\","
                << "\"instruction\":\"nop\","
                << "\"instructionBytes\":\"90\"}"
                << (i + 1 < insnCount ? "," : "");
        }
        oss << "]}";
        return formatDapResponse(resSeq, reqSeq, command, true, oss.str());
    }

    if (command == "evaluate") {
        std::string expr = extractJsonString(json, "expression");
        std::string body = "{\"result\":\"0x0\",\"type\":\"uint64_t\",\"variablesReference\":0}";
        return formatDapResponse(resSeq, reqSeq, command, true, body);
    }

    if (command == "disconnect") {
        if (engine_ && engine_->activeTid() > 0) {
            engine_->detach();
        }
        isRunning_ = false;
        return formatDapResponse(resSeq, reqSeq, command, true, "{}");
    }

    // Default response for unhandled request
    return formatDapResponse(resSeq, reqSeq, command, true, "{}");
}

void DapServer::run(std::istream& in, std::ostream& out) {
    isRunning_ = true;

    while (isRunning_ && in.good()) {
        std::string line;
        if (!std::getline(in, line)) break;

        if (line.starts_with("Content-Length:")) {
            size_t colon = line.find(':');
            int len = std::stoi(line.substr(colon + 1));

            // Read the empty line \r
            std::string empty;
            std::getline(in, empty);

            std::vector<char> buffer(len);
            in.read(buffer.data(), len);
            std::string message(buffer.data(), len);

            std::string response = handleMessage(message);
            std::string framed = frameDapMessage(response);
            out << framed;
            out.flush();
        }
    }

    isRunning_ = false;
}

void DapServer::stop() {
    isRunning_ = false;
}

} // namespace edb_next
