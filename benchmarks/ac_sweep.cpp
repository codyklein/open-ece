#include <algorithm>
#include <chrono>
#include <iostream>
#include <openece/circuits/ac/sweep.hpp>
using namespace openece::circuits;
namespace ac = openece::circuits::ac;
int main() {
    // Grounded passive mesh with reactive links and independent source constraints.
    // Record real wall times, never assert machine-dependent timing in tests.
    for (std::size_t node_count : {std::size_t{3}, std::size_t{32}, ac::limits::nodes}) {
        ac::CircuitDefinition d{{{{0}, "g"}}, NodeId{0}, {}};
        std::uint32_t component = 0;
        for (std::size_t i = 1; i < node_count; ++i) {
            const auto id = static_cast<std::uint32_t>(i);
            d.nodes.push_back({{id}, "N" + std::to_string(i)});
            d.components.push_back(
                Resistor{{component++}, "R" + std::to_string(i), {id}, {0}, 1000});
            if (i > 1)
                d.components.push_back(
                    ac::Capacitor{{component++}, "C" + std::to_string(i), {id}, {id - 1}, 1e-6});
            if (i <= ac::limits::voltage_sources)
                d.components.push_back(
                    ac::VoltageSource{{component++}, "V" + std::to_string(i), {id}, {0}, {1, .5}});
        }
        const ac::Circuit circuit(d);
        const auto k = node_count - 1 + std::min(node_count - 1, ac::limits::voltage_sources);
        const auto points =
            std::min<std::size_t>(ac::limits::sweep_points,
                                  static_cast<std::size_t>(ac::limits::sweep_work / (k * k * k)));
        const auto begin = std::chrono::steady_clock::now();
        const auto result = ac::sweep_ac(circuit, {10, 10000, points});
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        std::size_t failures = 0;
        for (const auto& point : result.points)
            if (std::holds_alternative<CircuitError>(point.result))
                ++failures;
        std::cout << "unknowns=" << k << " points=" << points << " work=" << points * k * k * k
                  << " seconds=" << elapsed << " failures=" << failures << '\n';
        if (failures)
            return 1;
    }
}
