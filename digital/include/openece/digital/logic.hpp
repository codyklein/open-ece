#pragma once

#include <cstdint>
#include <span>

namespace openece::digital {
enum class LogicValue : std::uint8_t { zero, one };
enum class GateKind { Not, And, Or, Nand, Nor, Xor, Xnor };

// NOT takes one pin; all other gates take one or more. XOR is odd parity.
// Invalid enum values and arities throw std::invalid_argument.
LogicValue evaluate_gate(GateKind kind, std::span<const LogicValue> inputs);
} // namespace openece::digital
