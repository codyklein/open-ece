#include "phase_input.hpp"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace openece::gui {
namespace {
double number(const QString& text) {
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        throw std::invalid_argument("Phase numbers must be finite and representable");
    }
    return value;
}
} // namespace

double parse_phase(const QString& text, bool in_radians) {
    if (text.size() > 128) {
        throw std::invalid_argument("Phase input is limited to 128 characters");
    }
    // Whitespace separates tokens; it must not join digits or turn 'p i' into 'pi'.
    static const QString decimal = R"((?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)";
    static const QRegularExpression plain("\\A\\s*([+-]?)\\s*(" + decimal + ")\\s*\\z");
    static const QRegularExpression multiple("\\A\\s*([+-]?)\\s*(?:(" + decimal +
                                                 ")\\s*\\*?\\s*)?(?:pi|π)"
                                                 "\\s*(?:/\\s*(" +
                                                 decimal + "))?\\s*\\z",
                                             QRegularExpression::CaseInsensitiveOption);

    double value = 0.0;
    const auto literal = plain.match(text);
    if (literal.hasMatch()) {
        value = number(literal.captured(2));
        if (literal.captured(1) == "-") {
            value = -value;
        }
    } else {
        const auto expression = multiple.match(text);
        if (!in_radians || !expression.hasMatch()) {
            throw std::invalid_argument(
                in_radians
                    ? "Invalid phase: use a decimal or a multiple of pi, such as -pi/2 or 3*pi/4"
                    : "Invalid phase: Degrees accepts decimals only; select Radians for pi "
                      "expressions");
        }
        const double coefficient =
            expression.captured(2).isEmpty() ? 1.0 : number(expression.captured(2));
        const double divisor =
            expression.captured(3).isEmpty() ? 1.0 : number(expression.captured(3));
        if (divisor <= 0.0) {
            throw std::invalid_argument("Phase divisor must be greater than zero");
        }
        value = (coefficient / divisor) * std::numbers::pi;
        if (expression.captured(1) == "-") {
            value = -value;
        }
    }
    const double limit = in_radians ? 2.0 * std::numbers::pi : 360.0;
    if (!std::isfinite(value) || std::abs(value) > limit) {
        throw std::invalid_argument(in_radians ? "Phase must be between -2*pi and 2*pi radians"
                                               : "Phase must be between -360 and 360 degrees");
    }
    return in_radians ? value : (value / 180.0) * std::numbers::pi;
}

QString format_phase(double radians, bool in_radians) {
    // Input was range-checked; clamp only conversion roundoff at the degree endpoints.
    const double value =
        in_radians ? radians : std::clamp((radians / std::numbers::pi) * 180.0, -360.0, 360.0);
    return QString::number(value, 'g', 17);
}

} // namespace openece::gui
