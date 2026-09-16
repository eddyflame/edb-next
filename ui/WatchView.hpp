#pragma once

#include "core/Types.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <memory>
#include <vector>
#include <string>

namespace edb_next {

class DebugSession;

struct WatchItem {
    std::string expression;
    std::optional<uint64_t> lastValue;
    bool hasChanged{false};
};

class WatchView : public QWidget {
    Q_OBJECT

public:
    explicit WatchView(QWidget* parent = nullptr);
    ~WatchView() override = default;

    void setSession(std::shared_ptr<DebugSession> session);
    void addWatch(const std::string& expr);
    void refresh();

    [[nodiscard]] std::vector<std::string> watchExpressions() const {
        std::vector<std::string> res;
        for (const auto& w : watches_) res.push_back(w.expression);
        return res;
    }
    void clearWatches() {
        onClearClicked();
    }
    void addWatchExpression(const QString& expr) {
        addWatch(expr.toStdString());
    }

private Q_SLOTS:
    void onAddClicked();
    void onRemoveClicked();
    void onClearClicked();

private:
    void setupUi();

    std::shared_ptr<DebugSession> session_;
    std::vector<WatchItem> watches_;

    QTableWidget* tableWatches_{nullptr};
    QLineEdit* txtNewExpr_{nullptr};
    QPushButton* btnAdd_{nullptr};
    QPushButton* btnRemove_{nullptr};
    QPushButton* btnClear_{nullptr};
};

} // namespace edb_next
