#include <openece/digital/logic.hpp>
#include <stdexcept>

namespace openece::digital {
LogicValue evaluate_gate(GateKind kind, std::span<const LogicValue> inputs) {
    if (inputs.empty() || (kind == GateKind::Not && inputs.size() != 1)) {
        throw std::invalid_argument("Invalid gate input count");
    }
    bool all = true, any = false, parity = false;
    for (auto input : inputs) {
        if (input != LogicValue::zero && input != LogicValue::one) {
            throw std::invalid_argument("Logic value must be zero or one");
        }
        const bool high = input == LogicValue::one;
        all = all && high;
        any = any || high;
        parity = parity != high;
    }
    bool result;
    switch (kind) {
    case GateKind::Not:
        result = !any;
        break;
    case GateKind::And:
        result = all;
        break;
    case GateKind::Or:
        result = any;
        break;
    case GateKind::Nand:
        result = !all;
        break;
    case GateKind::Nor:
        result = !any;
        break;
    case GateKind::Xor:
        result = parity;
        break;
    case GateKind::Xnor:
        result = !parity;
        break;
    default:
        throw std::invalid_argument("Unknown gate kind");
    }
    return result ? LogicValue::one : LogicValue::zero;
}
} // namespace openece::digital
