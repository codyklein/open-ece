#pragma once
#include <openece/project/model.hpp>
#include <type_traits>
namespace openece::project::detail {
struct Rule {
    std::size_t count = limits::array_items, bytes = limits::cell_bytes, utf16 = limits::cell_bytes;
    std::string_view tokens;
};
constexpr Rule choices(std::string_view values) { return {0, 64, 64, values}; }
constexpr Rule text(std::size_t units, std::size_t bytes = limits::string_bytes) {
    return {0, bytes, units, {}};
}
constexpr Rule rows(std::size_t count) { return {count, 0, 0, {}}; }
constexpr Rule quantity(std::string_view units, std::size_t chars = 128) {
    return {0, limits::string_bytes, chars, units};
}
// One explicit required-field list shared by strict reading, writing and model validation.
// No implicit DTO conversion or missing-field defaults are used by the codec.
template <class A, class T> void fields(A& a, T& v, Rule context = {}) {
    using V = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<V, Quantity>) {
        a.field("text", v.text, text(context.utf16, context.bytes));
        a.field("unit", v.unit, choices(context.tokens));
    } else if constexpr (std::is_same_v<V, SignalsDraft>) {
        a.field("amplitude", v.amplitude, quantity("1"));
        a.field("frequency", v.frequency, quantity("Hz"));
        a.field("phase", v.phase, quantity("deg|rad", 32767));
        a.field("sample_rate", v.sample_rate, quantity("Hz"));
        a.field("duration", v.duration, quantity("s"));
        a.field("cutoff", v.cutoff, quantity("Hz"));
        a.field("window", v.window, choices("rectangular|hann_periodic"));
        a.field("filter", v.filter, choices("off|fir_lowpass"));
        a.field("taps_text", v.taps_text, text(128));
        a.field("selected_tab", v.selected_tab, choices("signals|help|response"));
    } else if constexpr (std::is_same_v<V, DigitalInput>) {
        a.field("id", v.id);
        a.field("name", v.name);
        a.field("high", v.high);
    } else if constexpr (std::is_same_v<V, DigitalGate>) {
        a.field("id", v.id);
        a.field("kind", v.kind, choices("not|and|or|nand|nor|xor|xnor"));
        a.field("pins", v.pins, rows(limits::digital_pins));
        a.field("pin_count_text", v.pin_count_text, text(128));
    } else if constexpr (std::is_same_v<V, DigitalOutput>) {
        a.field("name", v.name);
        a.field("source", v.source);
    } else if constexpr (std::is_same_v<V, CombinationalDraft>) {
        a.field("next_id", v.next_id);
        a.field("inputs", v.inputs, rows(limits::digital_inputs));
        a.field("gates", v.gates, rows(limits::digital_elements));
        a.field("outputs", v.outputs, rows(limits::digital_outputs));
    } else if constexpr (std::is_same_v<V, ClockDraft>) {
        a.field("first_edge_text", v.first_edge_text);
        a.field("high_text", v.high_text);
        a.field("low_text", v.low_text);
        a.field("unit", v.unit, choices("ps"));
    } else if constexpr (std::is_same_v<V, TimingInput>) {
        a.field("id", v.id);
        a.field("name", v.name);
        a.field("initial_text", v.initial_text);
        a.field("clock", v.clock);
    } else if constexpr (std::is_same_v<V, TimingElement>) {
        a.field("id", v.id);
        a.field("kind", v.kind,
                choices("not|and|or|nand|nor|xor|xnor|sr_latch|d_latch|dff_rising|dff_falling"));
        a.field("pins_text", v.pins_text);
        a.field("delay", v.delay, quantity("ps", 4096));
        a.field("initial_q_text", v.initial_q_text);
    } else if constexpr (std::is_same_v<V, TimingOutput>) {
        a.field("name", v.name);
        a.field("source_text", v.source_text);
    } else if constexpr (std::is_same_v<V, Stimulus>) {
        a.field("time", v.time, quantity("ps", 4096));
        a.field("input_text", v.input_text);
        a.field("value_text", v.value_text);
    } else if constexpr (std::is_same_v<V, TimingDraft>) {
        a.field("next_id", v.next_id);
        a.field("inputs", v.inputs, rows(limits::digital_inputs));
        a.field("elements", v.elements, rows(limits::digital_elements));
        a.field("outputs", v.outputs, rows(limits::digital_outputs));
        a.field("stimuli", v.stimuli, rows(limits::timing_stimuli));
        a.field("horizon", v.horizon, quantity("ps", 13));
        a.field("observed_text", v.observed_text, text(192));
        a.field("display_unit", v.display_unit, choices("ps|ns|us|ms|s"));
        a.field("selected_tab", v.selected_tab, choices("inputs|elements|outputs|stimuli"));
    } else if constexpr (std::is_same_v<V, DigitalDraft>) {
        a.field("combinational", v.combinational);
        a.field("timing", v.timing);
        a.field("copy_delay", v.copy_delay, quantity("ps"));
        a.field("selected_tab", v.selected_tab, choices("combinational|timing"));
    } else if constexpr (std::is_same_v<V, Node>) {
        a.field("id", v.id);
        a.field("name", v.name);
    } else if constexpr (std::is_same_v<V, DcComponent> || std::is_same_v<V, AcComponent>) {
        a.field("id", v.id);
        a.field("name", v.name);
        if constexpr (std::is_same_v<V, DcComponent>)
            a.field("kind", v.kind, choices("resistor|voltage_source|current_source"));
        else
            a.field("kind", v.kind,
                    choices("resistor|capacitor|inductor|voltage_source|current_source"));
        a.field("positive", v.positive);
        a.field("negative", v.negative);
        std::string_view units = "ohm|kohm|Mohm";
        if (v.kind == "resistor" && std::is_same_v<V, DcComponent>)
            units = "ohm|kohm|Mohm|mohm";
        else if (v.kind == "capacitor")
            units = "F|uF|nF|pF";
        else if (v.kind == "inductor")
            units = "H|mH|uH";
        else if (v.kind == "voltage_source")
            units = "V|mV|uV";
        else if (v.kind == "current_source")
            units = "A|mA|uA";
        a.field("value", v.value, quantity(units));
        if constexpr (std::is_same_v<V, AcComponent>)
            a.field("phase", v.phase, quantity("deg"));
    } else if constexpr (std::is_same_v<V, DcDraft> || std::is_same_v<V, AcDraft>) {
        a.field("next_node", v.next_node);
        a.field("next_component", v.next_component);
        a.field("nodes", v.nodes, rows(limits::circuit_nodes));
        a.field("components", v.components, rows(limits::circuit_components));
        a.field("ground", v.ground);
        if constexpr (std::is_same_v<V, AcDraft>) {
            a.field("frequency", v.frequency, quantity("Hz|kHz|MHz|GHz"));
            a.field("start", v.start, quantity("Hz|kHz|MHz|GHz"));
            a.field("stop", v.stop, quantity("Hz|kHz|MHz|GHz"));
            a.field("count_text", v.count_text, text(128));
            a.field("spacing", v.spacing, choices("linear|logarithmic"));
            a.field("mode", v.mode, choices("transfer|absolute"));
            a.field("probe_positive", v.probe_positive);
            a.field("probe_negative", v.probe_negative);
            a.field("reference_source", v.reference_source);
            a.field("selected_tab", v.selected_tab, choices("editor|single|sweep|help"));
        } else
            a.field("selected_tab", v.selected_tab, choices("editor|help"));
    } else if constexpr (std::is_same_v<V, CircuitsDraft>) {
        a.field("dc", v.dc);
        a.field("ac", v.ac);
        a.field("selected_tab", v.selected_tab, choices("dc|ac"));
    } else if constexpr (std::is_same_v<V, CommunicationsDraft>) {
        a.field("modulation", v.modulation, choices("bpsk|qpsk"));
        a.field("source_mode", v.source_mode, choices("random|manual"));
        a.field("manual_bits_text", v.manual_bits_text, text(131072));
        a.field("bit_count_text", v.bit_count_text, text(128));
        a.field("samples_text", v.samples_text, text(128));
        a.field("symbol_rate", v.symbol_rate, quantity("symbol/s|ksymbol/s|Msymbol/s", 32767));
        a.field("bit_seed_text", v.bit_seed_text, text(64));
        a.field("noise_seed_text", v.noise_seed_text, text(64));
        a.field("noise_enabled", v.noise_enabled);
        a.field("eb_n0", v.eb_n0, quantity("dB", 32767));
        a.field("start", v.start, quantity("dB", 32767));
        a.field("stop", v.stop, quantity("dB", 32767));
        a.field("points_text", v.points_text, text(128));
        a.field("budget_text", v.budget_text, text(128));
        a.field("selected_tab", v.selected_tab, choices("waveforms|constellation|ber|help"));
    } else if constexpr (std::is_same_v<V, ProjectSnapshot>) {
        a.field("selected_domain", v.selected_domain,
                choices("signals|digital|circuits|communications"));
        a.field("signals", v.signals);
        a.field("digital", v.digital);
        a.field("circuits", v.circuits);
        a.field("communications", v.communications);
    } else {
        static_assert(sizeof(V) == 0, "Missing explicit project field specification");
    }
}
} // namespace openece::project::detail
