#include "DatabaseManager.hpp"
#include "DebugSession.hpp"
#include "AnnotationManager.hpp"
#include "BreakpointManager.hpp"
#include "PatchManager.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <sstream>
#include <iomanip>

namespace edb_next {

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

std::string DatabaseManager::defaultDatabasePath(const std::string& binaryPath) const {
    if (binaryPath.empty()) return "";

    QFileInfo fi(QString::fromStdString(binaryPath));
    QString db_name = fi.completeBaseName() + ".edb_db";

    // Store in same directory or in ~/.config/edb-next/databases/
    QString local_db = fi.absoluteDir().filePath(db_name);
    return local_db.toStdString();
}

bool DatabaseManager::saveToFile(const std::string& filepath, const DatabaseProject& project) {
    QJsonObject root;
    root["version"] = 1;
    root["binary_path"] = QString::fromStdString(project.binaryPath);
    root["notes"] = QString::fromStdString(project.notes);

    // Comments
    QJsonArray comments_arr;
    for (const auto& [addr, text] : project.comments) {
        QJsonObject c_obj;
        std::ostringstream ss;
        ss << "0x" << std::hex << addr;
        c_obj["address"] = QString::fromStdString(ss.str());
        c_obj["comment"] = QString::fromStdString(text);
        comments_arr.append(c_obj);
    }
    root["comments"] = comments_arr;

    // Labels
    QJsonArray labels_arr;
    for (const auto& [addr, text] : project.labels) {
        QJsonObject l_obj;
        std::ostringstream ss;
        ss << "0x" << std::hex << addr;
        l_obj["address"] = QString::fromStdString(ss.str());
        l_obj["label"] = QString::fromStdString(text);
        labels_arr.append(l_obj);
    }
    root["labels"] = labels_arr;

    // Bookmarks
    QJsonArray bm_arr;
    for (uint64_t addr : project.bookmarks) {
        std::ostringstream ss;
        ss << "0x" << std::hex << addr;
        bm_arr.append(QString::fromStdString(ss.str()));
    }
    root["bookmarks"] = bm_arr;

    // Breakpoints
    QJsonArray bp_arr;
    for (const auto& bp : project.breakpoints) {
        QJsonObject bp_obj;
        std::ostringstream ss;
        ss << "0x" << std::hex << bp.address;
        bp_obj["address"] = QString::fromStdString(ss.str());
        bp_obj["type"] = QString::fromStdString(bp.type);
        bp_obj["condition"] = QString::fromStdString(bp.condition);
        bp_obj["log_format"] = QString::fromStdString(bp.logFormat);
        bp_obj["ignore_count"] = static_cast<int>(bp.ignoreCount);
        bp_obj["script_code"] = QString::fromStdString(bp.scriptCode);
        bp_obj["script_lang"] = QString::fromStdString(bp.scriptLanguage);
        bp_arr.append(bp_obj);
    }
    root["breakpoints"] = bp_arr;

    // Page Guards
    QJsonArray pg_arr;
    for (const auto& pg : project.pageGuards) {
        QJsonObject pg_obj;
        std::ostringstream ss;
        ss << "0x" << std::hex << pg.address;
        pg_obj["address"] = QString::fromStdString(ss.str());
        pg_obj["size"] = static_cast<int>(pg.size);
        pg_obj["access"] = QString::fromStdString(pg.access);
        pg_obj["comment"] = QString::fromStdString(pg.comment);
        pg_obj["condition"] = QString::fromStdString(pg.condition);
        pg_obj["script_code"] = QString::fromStdString(pg.scriptCode);
        pg_obj["script_lang"] = QString::fromStdString(pg.scriptLanguage);
        pg_arr.append(pg_obj);
    }
    root["page_guards"] = pg_arr;

    // Watches
    QJsonArray w_arr;
    for (const auto& w : project.watches) {
        w_arr.append(QString::fromStdString(w));
    }
    root["watches"] = w_arr;

    // Patches
    QJsonArray p_arr;
    for (const auto& p : project.patches) {
        QJsonObject p_obj;
        std::ostringstream ss;
        ss << "0x" << std::hex << p.address;
        p_obj["address"] = QString::fromStdString(ss.str());
        p_obj["original_hex"] = QString::fromStdString(p.originalHex);
        p_obj["patched_hex"] = QString::fromStdString(p.patchedHex);
        p_arr.append(p_obj);
    }
    root["patches"] = p_arr;

    QJsonDocument doc(root);
    QFile file(QString::fromStdString(filepath));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool DatabaseManager::loadFromFile(const std::string& filepath, DatabaseProject& project) {
    QFile file(QString::fromStdString(filepath));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    project.binaryPath = root["binary_path"].toString().toStdString();
    project.notes = root["notes"].toString().toStdString();

    // Comments
    project.comments.clear();
    QJsonArray comments_arr = root["comments"].toArray();
    for (const auto& val : comments_arr) {
        QJsonObject obj = val.toObject();
        uint64_t addr = obj["address"].toString().toULongLong(nullptr, 16);
        std::string text = obj["comment"].toString().toStdString();
        project.comments.emplace_back(addr, text);
    }

    // Labels
    project.labels.clear();
    QJsonArray labels_arr = root["labels"].toArray();
    for (const auto& val : labels_arr) {
        QJsonObject obj = val.toObject();
        uint64_t addr = obj["address"].toString().toULongLong(nullptr, 16);
        std::string text = obj["label"].toString().toStdString();
        project.labels.emplace_back(addr, text);
    }

    // Bookmarks
    project.bookmarks.clear();
    QJsonArray bm_arr = root["bookmarks"].toArray();
    for (const auto& val : bm_arr) {
        uint64_t addr = val.toString().toULongLong(nullptr, 16);
        project.bookmarks.push_back(addr);
    }

    // Breakpoints
    project.breakpoints.clear();
    QJsonArray bp_arr = root["breakpoints"].toArray();
    for (const auto& val : bp_arr) {
        QJsonObject obj = val.toObject();
        DatabaseBreakpointData bp;
        bp.address = obj["address"].toString().toULongLong(nullptr, 16);
        bp.type = obj["type"].toString().toStdString();
        bp.condition = obj["condition"].toString().toStdString();
        bp.logFormat = obj["log_format"].toString().toStdString();
        bp.ignoreCount = static_cast<uint32_t>(obj["ignore_count"].toInt());
        bp.scriptCode = obj["script_code"].toString().toStdString();
        bp.scriptLanguage = obj["script_lang"].toString().toStdString();
        if (bp.scriptLanguage.empty()) bp.scriptLanguage = "python";
        project.breakpoints.push_back(bp);
    }

    // Page Guards
    project.pageGuards.clear();
    QJsonArray pg_arr = root["page_guards"].toArray();
    for (const auto& val : pg_arr) {
        QJsonObject obj = val.toObject();
        DatabasePageGuardData pg;
        pg.address = obj["address"].toString().toULongLong(nullptr, 16);
        pg.size = static_cast<size_t>(obj["size"].toInt(1));
        pg.access = obj["access"].toString().toStdString();
        pg.comment = obj["comment"].toString().toStdString();
        pg.condition = obj["condition"].toString().toStdString();
        pg.scriptCode = obj["script_code"].toString().toStdString();
        pg.scriptLanguage = obj["script_lang"].toString().toStdString();
        if (pg.scriptLanguage.empty()) pg.scriptLanguage = "python";
        project.pageGuards.push_back(pg);
    }

    // Watches
    project.watches.clear();
    QJsonArray w_arr = root["watches"].toArray();
    for (const auto& val : w_arr) {
        project.watches.push_back(val.toString().toStdString());
    }

    // Patches
    project.patches.clear();
    QJsonArray p_arr = root["patches"].toArray();
    for (const auto& val : p_arr) {
        QJsonObject obj = val.toObject();
        DatabasePatchData p;
        p.address = obj["address"].toString().toULongLong(nullptr, 16);
        p.originalHex = obj["original_hex"].toString().toStdString();
        p.patchedHex = obj["patched_hex"].toString().toStdString();
        project.patches.push_back(p);
    }

    return true;
}

bool DatabaseManager::exportSession(std::shared_ptr<DebugSession> session,
                                    const PatchManager* patchMgr,
                                    const std::string& notes,
                                    const std::vector<std::string>& watches,
                                    const std::string& filepath) {
    if (!session) return false;

    DatabaseProject proj;
    proj.binaryPath = session->targetPath();
    proj.notes = notes;
    proj.watches = watches;

    // Comments, Labels & Bookmarks
    for (const auto& [addr, comment] : session->annotationManager().allComments()) {
        proj.comments.emplace_back(addr, comment);
    }
    for (const auto& [addr, label] : session->annotationManager().allLabels()) {
        proj.labels.emplace_back(addr, label);
    }
    for (const auto& addr : session->annotationManager().bookmarks()) {
        proj.bookmarks.push_back(addr.value());
    }

    // Breakpoints
    for (const auto& bp : session->breakpointManager().allBreakpoints()) {
        if (bp.isInternal) continue;
        std::string type_str = "Software";
        if (bp.type == BreakpointType::HardwareExecute) {
            type_str = "HwExecute";
        } else if (bp.type == BreakpointType::HardwareWrite) {
            type_str = "HwWrite";
        } else if (bp.type == BreakpointType::HardwareReadWrite) {
            type_str = "HwReadWrite";
        }

        proj.breakpoints.push_back(DatabaseBreakpointData{
            .address = bp.address.value(),
            .type = type_str,
            .condition = bp.condition,
            .logFormat = bp.logFormat,
            .ignoreCount = bp.ignoreCount,
            .scriptCode = bp.scriptCode,
            .scriptLanguage = bp.scriptLanguage
        });
    }

    // Page Guards
    for (const auto& pg : session->pageGuardManager().allGuards()) {
        proj.pageGuards.push_back(DatabasePageGuardData{
            .address = pg.address.value(),
            .size = pg.size,
            .access = pageGuardAccessToString(pg.access),
            .comment = pg.comment,
            .condition = pg.condition,
            .scriptCode = pg.scriptCode,
            .scriptLanguage = pg.scriptLanguage
        });
    }

    // Patches
    if (patchMgr) {
        for (const auto& patch : patchMgr->patches()) {
            QByteArray orig(reinterpret_cast<const char*>(patch.originalBytes.data()), patch.originalBytes.size());
            QByteArray patched(reinterpret_cast<const char*>(patch.patchedBytes.data()), patch.patchedBytes.size());
            proj.patches.push_back(DatabasePatchData{
                .address = patch.address.value(),
                .originalHex = orig.toHex().toStdString(),
                .patchedHex = patched.toHex().toStdString()
            });
        }
    }

    return saveToFile(filepath, proj);
}

bool DatabaseManager::importSession(std::shared_ptr<DebugSession> session,
                                    PatchManager* patchMgr,
                                    std::string& outNotes,
                                    std::vector<std::string>& outWatches,
                                    const std::string& filepath) {
    if (!session) return false;

    DatabaseProject proj;
    if (!loadFromFile(filepath, proj)) {
        return false;
    }

    outNotes = proj.notes;
    outWatches = proj.watches;

    // Restore comments, labels & bookmarks
    for (const auto& [addr, text] : proj.comments) {
        session->annotationManager().setComment(Address(addr), text);
    }
    for (const auto& [addr, text] : proj.labels) {
        session->annotationManager().setLabel(Address(addr), text);
    }
    for (uint64_t addr : proj.bookmarks) {
        session->annotationManager().setBookmark(Address(addr), true);
    }

    // Restore breakpoints
    for (const auto& bp : proj.breakpoints) {
        Address addr(bp.address);
        if (bp.type == "HwExecute") {
            session->addHardwareBreakpoint(addr, HardwareBpType::Execute);
        } else if (bp.type == "HwWrite") {
            session->addHardwareBreakpoint(addr, HardwareBpType::Write);
        } else if (bp.type == "HwReadWrite") {
            session->addHardwareBreakpoint(addr, HardwareBpType::ReadWrite);
        } else {
            session->addBreakpoint(addr);
        }

        if (!bp.condition.empty()) {
            session->breakpointManager().setBreakpointCondition(addr, bp.condition);
        }
        if (bp.ignoreCount > 0) {
            session->breakpointManager().setBreakpointIgnoreCount(addr, bp.ignoreCount);
        }
        if (!bp.logFormat.empty()) {
            session->breakpointManager().setBreakpointLogOnly(addr, true, bp.logFormat);
        }
        if (!bp.scriptCode.empty()) {
            session->breakpointManager().setBreakpointScript(addr, bp.scriptCode, bp.scriptLanguage);
        }
    }

    // Restore page guards
    for (const auto& pg : proj.pageGuards) {
        Address addr(pg.address);
        session->addPageGuard(addr, pg.size, pageGuardAccessFromString(pg.access), pg.comment);
        auto* g = session->pageGuardManager().getGuardMutable(addr);
        if (g) {
            g->condition = pg.condition;
            g->scriptCode = pg.scriptCode;
            g->scriptLanguage = pg.scriptLanguage;
        }
    }

    // Restore patches
    if (patchMgr) {
        for (const auto& p : proj.patches) {
            Address patch_addr(p.address);
            QByteArray orig = QByteArray::fromHex(QByteArray::fromStdString(p.originalHex));
            QByteArray patched = QByteArray::fromHex(QByteArray::fromStdString(p.patchedHex));
            std::vector<uint8_t> obytes(orig.begin(), orig.end());
            std::vector<uint8_t> pbytes(patched.begin(), patched.end());
            patchMgr->addPatch(patch_addr, obytes, pbytes, "Restored Patch");
        }
    }
    return true;
}

} // namespace edb_next
