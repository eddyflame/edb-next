#include "NavigationBus.hpp"

namespace edb_next {

NavigationBus::NavigationBus(QObject* parent)
    : QObject(parent) {
}

NavigationBus::~NavigationBus() = default;

} // namespace edb_next
