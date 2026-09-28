#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <openece/project/model.hpp>
#include <string>
using namespace openece::project;
namespace {
std::string read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot read fixture " + path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
class ExampleCodec : public testing::TestWithParam<const char*> {};
TEST_P(ExampleCodec, SchemaOneAndSemanticRoundTrip) {
    const auto decoded = decode_project(read(std::string(OPENECE_SOURCE_DIR) + GetParam()));
    EXPECT_TRUE(decoded.ignored_fields.empty());
    const auto& p = decoded.snapshot;
    EXPECT_EQ(decode_project(encode_project(p)).snapshot, p);
    EXPECT_EQ(p.digital.combinational.inputs[0].id, Id{1});
    EXPECT_EQ(p.digital.combinational.gates[0].id, Id{3});
    EXPECT_EQ(p.digital.combinational.next_id, NextId{5});
    EXPECT_EQ(p.digital.timing.inputs[1].clock.unit, "ps");
    EXPECT_EQ(p.circuits.ac.reference_source, Reference{Id{0}});
    EXPECT_EQ(p.circuits.dc.components[0].value, (Quantity{"10", "V"}));
    EXPECT_EQ(p.communications.bit_seed_text, "1");
    EXPECT_EQ(p.communications.noise_seed_text, "2");
    if (std::string(GetParam()).find("incomplete") != std::string::npos) {
        EXPECT_EQ(p.signals.phase, (Quantity{"3π/", "rad"}));
        EXPECT_EQ(p.digital.combinational.gates[0].pins[1], Reference{Id{77}});
        EXPECT_EQ(p.digital.timing.elements[0].pins_text, "1,");
        EXPECT_EQ(p.circuits.dc.components[1].negative, Reference{Id{9}});
        EXPECT_EQ(p.circuits.dc.components[1].value.text, "1e-");
        EXPECT_EQ(p.communications.manual_bits_text, "01 1");
        EXPECT_EQ(p.circuits.ac.nodes[2].name, "π output 🚀");
    }
}
INSTANTIATE_TEST_SUITE_P(
    ShippedFiles, ExampleCodec,
    testing::Values("/examples/sine-fft-fir.openece", "/examples/half-adder.openece",
                    "/examples/dff-timing.openece", "/examples/dc-divider.openece",
                    "/examples/rc-lowpass.openece", "/examples/series-rlc.openece",
                    "/examples/bpsk-link-ber.openece", "/examples/qpsk-link-ber.openece",
                    "/examples/intentionally-incomplete.openece",
                    "/tests/fixtures/v0.9/complete.openece",
                    "/tests/fixtures/v0.9/incomplete.openece"));
} // namespace
