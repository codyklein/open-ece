#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace openece::project {
namespace limits {
inline constexpr std::size_t file_bytes = 8 * 1024 * 1024, depth = 32, values = 200000,
                             members = 128, text_bytes = 4 * 1024 * 1024, string_bytes = 512 * 1024,
                             key_bytes = 256, array_items = 10000, rows = 12000, name_bytes = 4096,
                             cell_bytes = 4096;
inline constexpr std::size_t digital_inputs = 8, digital_elements = 64, digital_outputs = 16,
                             digital_pins = 8, timing_stimuli = 10000, circuit_nodes = 32,
                             circuit_components = 128;
inline constexpr std::uint64_t exhausted_id = std::uint64_t{1} << 32;
} // namespace limits
enum class ErrorCode {
    malformed_json,
    invalid_utf8,
    duplicate_key,
    wrong_format,
    unsupported_version,
    missing_field,
    wrong_type,
    invalid_value,
    invalid_identity,
    duplicate_identity,
    invalid_allocator_state,
    resource_limit,
    read_failed,
    file_too_large,
    restore_failed,
    encode_failed,
    save_open_failed,
    save_write_failed,
    save_commit_failed
};
class Error : public std::runtime_error {
  public:
    Error(ErrorCode code, std::string path, std::string message,
          std::optional<std::size_t> offset = std::nullopt)
        : std::runtime_error(std::move(message)), code_(code), path_(std::move(path)),
          offset_(offset) {}
    ErrorCode code() const noexcept { return code_; }
    const std::string& path() const noexcept { return path_; }
    std::optional<std::size_t> offset() const noexcept { return offset_; }

  private:
    ErrorCode code_;
    std::string path_;
    std::optional<std::size_t> offset_;
};
struct Id {
    std::uint32_t value = 0;
    bool operator==(const Id&) const = default;
};
struct NextId {
    std::uint64_t value = 1;
    bool operator==(const NextId&) const = default;
};
using Reference = std::optional<Id>;
struct Quantity {
    std::string text, unit;
    bool operator==(const Quantity&) const = default;
};
struct SignalsDraft {
    Quantity amplitude{"1.0000", "1"}, frequency{"8.0000", "Hz"}, phase{"0", "deg"},
        sample_rate{"1024.0000", "Hz"}, duration{"1.000000", "s"}, cutoff{"64.000000", "Hz"};
    std::string window = "rectangular", filter = "off", taps_text = "63", selected_tab = "signals";
    bool operator==(const SignalsDraft&) const = default;
};
struct DigitalInput {
    Id id;
    std::string name;
    bool high = false;
    bool operator==(const DigitalInput&) const = default;
};
struct DigitalGate {
    Id id;
    std::string kind = "and";
    std::vector<Reference> pins;
    std::string pin_count_text = "2";
    bool operator==(const DigitalGate&) const = default;
};
struct DigitalOutput {
    std::string name;
    Reference source;
    bool operator==(const DigitalOutput&) const = default;
};
struct CombinationalDraft {
    NextId next_id;
    std::vector<DigitalInput> inputs;
    std::vector<DigitalGate> gates;
    std::vector<DigitalOutput> outputs;
    bool operator==(const CombinationalDraft&) const = default;
};
struct ClockDraft {
    std::string first_edge_text, high_text, low_text, unit = "ps";
    bool operator==(const ClockDraft&) const = default;
};
struct TimingInput {
    Id id;
    std::string name, initial_text;
    ClockDraft clock;
    bool operator==(const TimingInput&) const = default;
};
struct TimingElement {
    Id id;
    std::string kind = "not", pins_text;
    Quantity delay{"", "ps"};
    std::string initial_q_text;
    bool operator==(const TimingElement&) const = default;
};
struct TimingOutput {
    std::string name, source_text;
    bool operator==(const TimingOutput&) const = default;
};
struct Stimulus {
    Quantity time{"", "ps"};
    std::string input_text, value_text;
    bool operator==(const Stimulus&) const = default;
};
struct TimingDraft {
    NextId next_id;
    std::vector<TimingInput> inputs;
    std::vector<TimingElement> elements;
    std::vector<TimingOutput> outputs;
    std::vector<Stimulus> stimuli;
    Quantity horizon{"30000", "ps"};
    std::string observed_text, display_unit = "ns", selected_tab = "inputs";
    bool operator==(const TimingDraft&) const = default;
};
struct DigitalDraft {
    CombinationalDraft combinational;
    TimingDraft timing;
    Quantity copy_delay{"1000", "ps"};
    std::string selected_tab = "combinational";
    bool operator==(const DigitalDraft&) const = default;
};
struct Node {
    Id id;
    std::string name;
    bool operator==(const Node&) const = default;
};
struct DcComponent {
    Id id;
    std::string name, kind = "resistor";
    Reference positive, negative;
    Quantity value{"", "ohm"};
    bool operator==(const DcComponent&) const = default;
};
struct AcComponent {
    Id id;
    std::string name, kind = "resistor";
    Reference positive, negative;
    Quantity value{"", "ohm"}, phase{"0", "deg"};
    bool operator==(const AcComponent&) const = default;
};
struct DcDraft {
    NextId next_node{0}, next_component{0};
    std::vector<Node> nodes;
    std::vector<DcComponent> components;
    Reference ground;
    std::string selected_tab = "editor";
    bool operator==(const DcDraft&) const = default;
};
struct AcDraft {
    NextId next_node{0}, next_component{0};
    std::vector<Node> nodes;
    std::vector<AcComponent> components;
    Reference ground;
    Quantity frequency{"1", "Hz"}, start{"1", "Hz"}, stop{"100000", "Hz"};
    std::string count_text = "201", spacing = "logarithmic", mode = "transfer",
                selected_tab = "editor";
    Reference probe_positive, probe_negative, reference_source;
    bool operator==(const AcDraft&) const = default;
};
struct CircuitsDraft {
    DcDraft dc;
    AcDraft ac;
    std::string selected_tab = "dc";
    bool operator==(const CircuitsDraft&) const = default;
};
struct CommunicationsDraft {
    std::string modulation = "qpsk", source_mode = "random", manual_bits_text = "00011110",
                bit_count_text = "256", samples_text = "16", bit_seed_text = "1",
                noise_seed_text = "2", points_text = "7", budget_text = "100000",
                selected_tab = "waveforms";
    Quantity symbol_rate{"1", "ksymbol/s"}, eb_n0{"6", "dB"}, start{"-2", "dB"}, stop{"10", "dB"};
    bool noise_enabled = true;
    bool operator==(const CommunicationsDraft&) const = default;
};
struct ProjectSnapshot {
    std::string selected_domain = "signals";
    SignalsDraft signals;
    DigitalDraft digital;
    CircuitsDraft circuits;
    CommunicationsDraft communications;
    bool operator==(const ProjectSnapshot&) const = default;
};
struct DecodedProject {
    ProjectSnapshot snapshot;
    // JSON Pointer paths. Unknown optional fields are accepted, not retained on encode.
    std::vector<std::string> ignored_fields;
};
ProjectSnapshot default_project();
void validate_structure(const ProjectSnapshot&);
DecodedProject decode_project(std::string_view utf8);
std::string encode_project(const ProjectSnapshot&);
// Counter stays monotonic; skip declared AND dangling-reference IDs supplied in reserved.
// On exhaustion the counter becomes exhausted_id and allocation throws without wrapping.
Id allocate_id(NextId& next, std::span<const Id> reserved);
std::vector<Id> reserved_ids(const CombinationalDraft&);
std::vector<Id> reserved_ids(const TimingDraft&);
std::vector<Id> reserved_node_ids(const DcDraft&);
std::vector<Id> reserved_node_ids(const AcDraft&);
std::vector<Id> reserved_component_ids(const DcDraft&);
std::vector<Id> reserved_component_ids(const AcDraft&);
} // namespace openece::project
