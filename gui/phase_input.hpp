#pragma once

#include <QString>

namespace openece::gui {

// GUI input contract: signed decimal, or (in radians) a signed decimal multiple
// of pi/π optionally divided by a positive decimal. No general arithmetic.
// Returns radians; rejects malformed, nonfinite, or out-of-range input.
[[nodiscard]] double parse_phase(const QString& text, bool in_radians);

// Decimal text retaining double precision when changing the displayed unit.
[[nodiscard]] QString format_phase(double radians, bool in_radians);

} // namespace openece::gui
