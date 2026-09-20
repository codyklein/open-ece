#include <algorithm>
#include <charconv>
#include <openece/project/model.hpp>
#include <unordered_set>
namespace openece::project {
namespace {
void add(std::vector<Id>& v, Reference r) {
    if (r)
        v.push_back(*r);
}
void text_ids(std::vector<Id>& v, std::string_view text) {
    // Reserve each parseable decimal token even if other tokens in the same draft are invalid.
    // ASCII whitespace is the timing grammar; Unicode whitespace is handled below conservatively
    // by scanning every decimal run, so an invalid draft can never reconnect by accidental reuse.
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] < '0' || text[i] > '9') {
            ++i;
            continue;
        }
        const auto start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            ++i;
        std::uint32_t value = 0;
        auto result = std::from_chars(text.data() + start, text.data() + i, value);
        if (result.ec == std::errc{})
            v.push_back({value});
    }
}
template <class T> std::vector<Id> node_ids(const T& d) {
    std::vector<Id> v;
    for (auto& n : d.nodes)
        v.push_back(n.id);
    add(v, d.ground);
    for (auto& c : d.components) {
        add(v, c.positive);
        add(v, c.negative);
    }
    return v;
}
} // namespace
Id allocate_id(NextId& next, std::span<const Id> reserved) {
    if (next.value > limits::exhausted_id)
        throw Error(ErrorCode::invalid_allocator_state, "", "Invalid ID counter");
    std::unordered_set<std::uint32_t> used;
    for (auto id : reserved)
        used.insert(id.value);
    while (next.value < limits::exhausted_id &&
           used.contains(static_cast<std::uint32_t>(next.value)))
        ++next.value;
    if (next.value == limits::exhausted_id)
        throw Error(ErrorCode::resource_limit, "", "ID space exhausted");
    return {static_cast<std::uint32_t>(next.value++)};
}
std::vector<Id> reserved_ids(const CombinationalDraft& d) {
    std::vector<Id> v{{0}};
    for (auto& i : d.inputs)
        v.push_back(i.id);
    for (auto& g : d.gates) {
        v.push_back(g.id);
        for (auto p : g.pins)
            add(v, p);
    }
    for (auto& o : d.outputs)
        add(v, o.source);
    return v;
}
std::vector<Id> reserved_ids(const TimingDraft& d) {
    std::vector<Id> v;
    for (auto& i : d.inputs)
        v.push_back(i.id);
    for (auto& e : d.elements) {
        v.push_back(e.id);
        text_ids(v, e.pins_text);
    }
    for (auto& o : d.outputs)
        text_ids(v, o.source_text);
    for (auto& s : d.stimuli)
        text_ids(v, s.input_text);
    text_ids(v, d.observed_text);
    return v;
}
std::vector<Id> reserved_node_ids(const DcDraft& d) { return node_ids(d); }
std::vector<Id> reserved_node_ids(const AcDraft& d) {
    auto v = node_ids(d);
    add(v, d.probe_positive);
    add(v, d.probe_negative);
    return v;
}
std::vector<Id> reserved_component_ids(const DcDraft& d) {
    std::vector<Id> v;
    for (auto& c : d.components)
        v.push_back(c.id);
    return v;
}
std::vector<Id> reserved_component_ids(const AcDraft& d) {
    std::vector<Id> v;
    for (auto& c : d.components)
        v.push_back(c.id);
    add(v, d.reference_source);
    return v;
}
ProjectSnapshot default_project() {
    ProjectSnapshot p;
    p.digital.combinational = {NextId{5},
                               {{{1}, "A", false}, {{2}, "B", false}},
                               {{{3}, "xor", {Id{1}, Id{2}}}, {{4}, "and", {Id{1}, Id{2}}}},
                               {{"Sum", Id{3}}, {"Carry", Id{4}}}};
    auto& t = p.digital.timing;
    t.next_id = {4};
    t.inputs = {{{1}, "D", "0", {}}, {{2}, "CLK", "0", {"5000", "5000", "5000", "ps"}}};
    t.elements = {{{3}, "dff_rising", "1,2", {"1000", "ps"}, "0"}};
    t.outputs = {{"Q", "3"}};
    t.stimuli = {{{"2000", "ps"}, "1", "1"}, {{"12000", "ps"}, "1", "0"}};
    t.observed_text = "1,2,3";
    auto& d = p.circuits.dc;
    d.next_node = {3};
    d.next_component = {3};
    d.nodes = {{{0}, "Ground"}, {{1}, "Supply"}, {{2}, "Midpoint"}};
    d.ground = Id{0};
    d.components = {{{0}, "V1", "voltage_source", Id{1}, Id{0}, {"10", "V"}},
                    {{1}, "R1", "resistor", Id{1}, Id{2}, {"1000", "ohm"}},
                    {{2}, "R2", "resistor", Id{2}, Id{0}, {"1000", "ohm"}}};
    auto& ac = p.circuits.ac;
    ac.next_node = {3};
    ac.next_component = {3};
    ac.nodes = {{{0}, "Ground"}, {{1}, "Input"}, {{2}, "Output"}};
    ac.ground = Id{0};
    ac.components = {{{0}, "V1", "voltage_source", Id{1}, Id{0}, {"1", "V"}, {"0", "deg"}},
                     {{1}, "R1", "resistor", Id{1}, Id{2}, {"1", "kohm"}, {"0", "deg"}},
                     {{2}, "C1", "capacitor", Id{2}, Id{0}, {"1", "uF"}, {"0", "deg"}}};
    ac.frequency.text = "159.15494309189535";
    ac.start.text = "10";
    ac.stop.text = "10000";
    ac.probe_positive = Id{2};
    ac.probe_negative = Id{0};
    ac.reference_source = Id{0};
    return p;
}
} // namespace openece::project
