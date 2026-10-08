#include "transient_execution.hpp"
#include "circuit_draft_rows.hpp"
namespace openece::gui {
namespace {
using namespace circuits;
namespace tr = transient_core;
[[noreturn]] void invalid(const QString& field) {
    throw tr::Error(tr::ErrorCode::invalid_request, draft_text(field));
}
double number(const project::Quantity& q, const QString& field) {
    auto n = circuit_value_si(qt_text(q.text), unit_factor(qt_text(q.unit)));
    if (!n)
        invalid(field + ": enter a finite number in the selected unit.");
    return *n;
}
std::uint32_t reference(project::Reference r, const QString& field) {
    if (!r)
        invalid(field + ": select an explicit reference.");
    return r->value;
}
QString node_label(project::Reference r, const project::TransientDraft& d) {
    const auto id = reference(r, "Probe terminal");
    for (const auto& n : d.nodes)
        if (n.id.value == id)
            return qt_text(n.name) + " [" + QString::number(id) + "]";
    return "Missing node [" + QString::number(id) + "]";
}
} // namespace
project::TransientDraft transient_active_draft(project::TransientDraft d) {
    d.next_node = {};
    d.next_component = {};
    d.selected_tab = "editor";
    // Display units are frozen for a run, but are not numerical inputs.
    d.display_time_unit = "s";
    for (auto& c : d.components) {
        if (c.kind != "voltage_source" && c.kind != "current_source")
            c.source = {};
        else if (c.source.mode == "constant")
            c.source.points.clear();
    }
    if (d.initialization == "operating_point")
        d.initial_conditions.clear();
    for (auto& p : d.probes) {
        if (p.kind == "voltage")
            p.component.reset();
        else {
            p.positive.reset();
            p.negative.reset();
        }
    }
    return d;
}
TransientExecution transient_execution(const project::TransientDraft& d) {
    namespace tr = transient_core;
    tr::CircuitDefinition definition;
    for (const auto& n : d.nodes)
        definition.nodes.push_back({{n.id.value}, n.name});
    definition.ground = circuits::NodeId{reference(d.ground, "Ground")};
    for (const auto& c : d.components) {
        const auto field = qt_text(c.name) + " [" + QString::number(c.id.value) + "]";
        circuits::ComponentId id{c.id.value};
        circuits::NodeId p{reference(c.positive, field + " positive terminal")};
        circuits::NodeId n{reference(c.negative, field + " negative terminal")};
        const auto value = number(c.value, field);
        if (c.kind == "resistor")
            definition.components.emplace_back(circuits::Resistor{id, c.name, p, n, value});
        else if (c.kind == "capacitor")
            definition.components.emplace_back(tr::Capacitor{id, c.name, p, n, value});
        else if (c.kind == "inductor")
            definition.components.emplace_back(tr::Inductor{id, c.name, p, n, value});
        else {
            tr::Source source{value,
                              {},
                              c.source.mode == "linear" ? tr::Interpolation::linear
                                                        : tr::Interpolation::hold};
            if (c.source.mode != "constant")
                for (const auto& point : c.source.points)
                    source.points.push_back(
                        {number(point.time, field + " source time"),
                         number({point.value_text, c.value.unit}, field + " source amplitude")});
            if (c.kind == "voltage_source")
                definition.components.emplace_back(
                    tr::VoltageSource{id, c.name, p, n, std::move(source)});
            else if (c.kind == "current_source")
                definition.components.emplace_back(
                    tr::CurrentSource{id, c.name, p, n, std::move(source)});
            else
                invalid(field + ": unsupported component kind.");
        }
    }
    tr::Request request;
    request.stop_seconds = number(d.stop, "Stop time");
    request.maximum_step_seconds = number(d.maximum_step, "Maximum step");
    if (d.initialization == "specified_storage") {
        request.initial.mode = tr::Initialization::specified_storage;
        for (const auto& i : d.initial_conditions) {
            circuits::ComponentId id{reference(i.component, "Initial-condition component")};
            const double value = number(i.value, "Initial condition");
            if (i.kind == "capacitor_voltage")
                request.initial.capacitor_voltages.push_back({id, value});
            else
                request.initial.inductor_currents.push_back({id, value});
        }
    }
    std::vector<TransientProbeLabel> labels;
    for (const auto& p : d.probes) {
        if (p.kind == "voltage") {
            labels.push_back({qt_text(p.name) + ": " + node_label(p.positive, d) + " → " +
                                  node_label(p.negative, d) + " (V)",
                              true, request.voltages.size()});
            request.voltages.push_back({{reference(p.positive, "Voltage probe positive")},
                                        {reference(p.negative, "Voltage probe negative")}});
        } else {
            const auto id = reference(p.component, "Current probe component");
            QString component = "Missing component [" + QString::number(id) + "]";
            for (const auto& c : d.components)
                if (c.id.value == id)
                    component = qt_text(c.name) + " [" + QString::number(id) +
                                "]: " + node_label(c.positive, d) + " → " +
                                node_label(c.negative, d);
            labels.push_back(
                {qt_text(p.name) + ": " + component + " (A)", false, request.currents.size()});
            request.currents.push_back({{id}});
        }
    }
    return {tr::Circuit(std::move(definition)), std::move(request), std::move(labels),
            qt_text(d.display_time_unit), unit_factor(qt_text(d.display_time_unit))};
}
} // namespace openece::gui
