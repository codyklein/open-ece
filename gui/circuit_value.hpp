#pragma once
#include <QString>
#include <optional>
namespace openece::gui {
// Decimal/scientific C-locale text only; no grouping, expressions, or embedded suffixes.
std::optional<double> circuit_value_si(const QString& text, double unit_scale);
} // namespace openece::gui
