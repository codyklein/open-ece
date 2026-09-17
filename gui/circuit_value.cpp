#include "circuit_value.hpp"
#include <QLocale>
#include <QRegularExpression>
#include <cmath>
namespace openece::gui {
std::optional<double> circuit_value_si(const QString& text, double unit_scale) {
    static const QRegularExpression syntax(
        QStringLiteral(R"(^[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?$)"));
    const auto trimmed = text.trimmed();
    if (!syntax.match(trimmed).hasMatch() || !std::isfinite(unit_scale) || unit_scale <= 0)
        return std::nullopt;
    bool ok = false;
    const double value = QLocale::c().toDouble(trimmed, &ok);
    const double si = value * unit_scale;
    if (!ok || !std::isfinite(si) || (value != 0 && si == 0))
        return std::nullopt;
    return si;
}
} // namespace openece::gui
