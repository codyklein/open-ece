#include "mna.hpp"
#include <Eigen/LU>
#include <cmath>
#include <openece/circuits/dc.hpp>
#include <type_traits>
namespace openece::circuits {
namespace {
[[noreturn]] void numerical_failure() {
    throw CircuitError(ErrorCode::numerical_failure,
                       "DC solution failed finite-value or residual checks.");
}
} // namespace
DcSolution solve_dc(const Circuit& circuit) {
    detail::check_topology(circuit);
    auto system = detail::assemble(circuit);
    const auto size = system.size();
    const auto k = static_cast<Eigen::Index>(size);
    DcSolution result;
    Eigen::VectorXd x(k);
    if (size) {
        Eigen::MatrixXd a(k, k);
        Eigen::VectorXd b(k);
        for (Eigen::Index i = 0; i < k; ++i) {
            b[i] = system.rhs[static_cast<std::size_t>(i)];
            for (Eigen::Index j = 0; j < k; ++j)
                a(i, j) =
                    system.matrix[static_cast<std::size_t>(i) * size + static_cast<std::size_t>(j)];
        }
        Eigen::VectorXd columns = Eigen::VectorXd::Ones(k);
        // A'=Dr*A*Dc, b'=Dr*b; physical x=Dc*y. Scaling never depends on b.
        for (int pass = 0; pass < policy::equilibration_passes; ++pass) {
            for (Eigen::Index i = 0; i < k; ++i) {
                double scale = a.row(i).cwiseAbs().maxCoeff();
                if (scale == 0)
                    throw CircuitError(ErrorCode::rank_deficient,
                                       "Zero MNA equation after topology checks.");
                a.row(i) /= scale;
                b[i] /= scale;
            }
            for (Eigen::Index j = 0; j < k; ++j) {
                double scale = a.col(j).cwiseAbs().maxCoeff();
                if (scale == 0)
                    throw CircuitError(ErrorCode::rank_deficient,
                                       "Zero MNA column after topology checks.");
                a.col(j) /= scale;
                columns[j] /= scale;
            }
        }
        if (!a.allFinite() || !b.allFinite() || !columns.allFinite())
            numerical_failure();
        Eigen::FullPivLU<Eigen::MatrixXd> lu(a);
        lu.setThreshold(policy::rank_threshold(size));
        if (lu.rank() != k)
            throw CircuitError(ErrorCode::rank_deficient,
                               "MNA matrix is numerically rank deficient at the configured scaled "
                               "pivot threshold.");
        result.quality.scaled_reciprocal_condition = lu.rcond();
        if (!std::isfinite(result.quality.scaled_reciprocal_condition) ||
            result.quality.scaled_reciprocal_condition < policy::reciprocal_condition_min)
            throw CircuitError(ErrorCode::ill_conditioned,
                               "MNA matrix is too ill-conditioned under the numerical policy.");
        x = columns.cwiseProduct(lu.solve(b));
        if (!x.allFinite())
            numerical_failure();
        for (std::size_t i = 0; i < size; ++i) {
            double residual = -system.rhs[i], scale = std::abs(system.rhs[i]);
            for (std::size_t j = 0; j < size; ++j) {
                double term = system.matrix[i * size + j] * x[static_cast<Eigen::Index>(j)];
                residual += term;
                scale += std::abs(term);
            }
            if (!std::isfinite(residual) || !std::isfinite(scale))
                numerical_failure();
            double error = scale == 0
                               ? (residual == 0 ? 0 : std::numeric_limits<double>::infinity())
                               : std::abs(residual) / scale;
            result.quality.backward_error = std::max(result.quality.backward_error, error);
        }
        if (result.quality.backward_error > policy::backward_threshold(size))
            numerical_failure();
    }
    const auto& d = circuit.definition();
    for (const auto& n : d.nodes) {
        double value = 0;
        if (n.id != *d.ground)
            value = x[std::find(system.voltage_nodes.begin(), system.voltage_nodes.end(), n.id) -
                      system.voltage_nodes.begin()];
        result.node_voltages.push_back({n.id, value});
    }
    for (std::size_t i = 0; i < system.voltage_sources.size(); ++i)
        result.voltage_source_currents.push_back(
            {system.voltage_sources[i],
             x[static_cast<Eigen::Index>(system.voltage_nodes.size() + i)]});
    // Independent branch checks include the omitted ground KCL equation.
    std::vector<double> balance(d.nodes.size()), magnitude(d.nodes.size());
    auto index = [&](NodeId id) {
        return static_cast<std::size_t>(std::find_if(d.nodes.begin(), d.nodes.end(),
                                                     [&](const Node& n) { return n.id == id; }) -
                                        d.nodes.begin());
    };
    for (const auto& part : d.components)
        std::visit(
            [&](const auto& c) {
                using T = std::decay_t<decltype(c)>;
                auto p = index(c.positive), n = index(c.negative);
                double vp = result.node_voltages[p].voltage_volts,
                       vn = result.node_voltages[n].voltage_volts, current;
                if constexpr (std::is_same_v<T, Resistor>)
                    current = (vp - vn) / c.resistance_ohms;
                else if constexpr (std::is_same_v<T, CurrentSource>)
                    current = c.current_amperes;
                else {
                    auto found = std::find_if(result.voltage_source_currents.begin(),
                                              result.voltage_source_currents.end(),
                                              [&](const auto& v) { return v.source == c.id; });
                    current = found->current_amperes;
                    double error = std::abs(vp - vn - c.voltage_volts);
                    result.quality.max_constraint_error_volts =
                        std::max(result.quality.max_constraint_error_volts, error);
                    if (error > policy::constraint_absolute_volts +
                                    policy::physical_relative *
                                        (std::abs(vp) + std::abs(vn) + std::abs(c.voltage_volts)))
                        numerical_failure();
                }
                if (!std::isfinite(current))
                    numerical_failure();
                balance[p] += current;
                balance[n] -= current;
                magnitude[p] += std::abs(current);
                magnitude[n] += std::abs(current);
            },
            part);
    for (std::size_t i = 0; i < balance.size(); ++i) {
        double error = std::abs(balance[i]);
        result.quality.max_kcl_error_amperes =
            std::max(result.quality.max_kcl_error_amperes, error);
        if (!std::isfinite(error) ||
            error > policy::kcl_absolute_amperes + policy::physical_relative * magnitude[i])
            numerical_failure();
    }
    return result;
}
} // namespace openece::circuits
