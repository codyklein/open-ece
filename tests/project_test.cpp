#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <limits>
#include <nlohmann/json.hpp>
#include <openece/project/model.hpp>
using namespace openece::project;
using Json = nlohmann::json;
namespace {
Json document() { return Json::parse(encode_project(default_project())); }
void rejected(std::string_view bytes, ErrorCode code) {
    try {
        (void)decode_project(bytes);
        FAIL() << "Accepted invalid project";
    } catch (const Error& e) {
        EXPECT_EQ(e.code(), code) << e.what() << " at " << e.path();
    }
}
void roundtrip(const ProjectSnapshot& p) {
    auto bytes = encode_project(p);
    auto loaded = decode_project(bytes);
    EXPECT_EQ(loaded.snapshot, p);
    EXPECT_TRUE(loaded.ignored_fields.empty());
    EXPECT_EQ(encode_project(loaded.snapshot), bytes);
}
} // namespace
TEST(Project, CompleteFixtureAndAllDomainsRoundTrip) {
    std::ifstream input(OPENECE_PROJECT_FIXTURE, std::ios::binary);
    ASSERT_TRUE(input);
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    auto p = decode_project(bytes).snapshot;
    EXPECT_EQ(p.signals.phase.text, "3π/");
    EXPECT_EQ(p.circuits.dc.components[1].value.text, "1e-");
    EXPECT_EQ(p.digital.timing.elements[0].pins_text, "1,");
    EXPECT_EQ(p.communications.manual_bits_text, "01 1");
    EXPECT_EQ(p.circuits.ac.nodes[2].name, "π output 🚀");
    roundtrip(p);
    roundtrip(default_project());
}
TEST(Project, InvalidDraftIsNotAnInvalidStructure) {
    auto p = default_project();
    p.digital.combinational.inputs[0].name = "";
    p.digital.combinational.inputs[1].name = "";
    p.digital.combinational.gates[0].pins = {Id{999}, std::nullopt};
    p.digital.combinational.gates[1].pins.clear();
    p.digital.timing.elements[0].pins_text = " 1,missing,, -2 ";
    p.digital.timing.horizon.text = "";
    p.digital.timing.stimuli.push_back({{"huge?", "ps"}, "no id", ""});
    p.circuits.dc.ground = Id{300};
    p.circuits.dc.components[0].positive = Id{500};
    p.circuits.dc.components[0].value.text = "NaN";
    p.circuits.ac.components[0].phase.text = "1e999";
    p.communications.bit_count_text = "3";
    p.communications.noise_seed_text = "18446744073709551616";
    p.communications.budget_text = "999999999999999999999";
    p.signals.phase.text = "pi/";
    roundtrip(p);
}
TEST(Project, ExactWhitespaceDisabledFieldsAndEmbeddedUnicode) {
    auto p = default_project();
    p.signals.filter = "off";
    p.signals.cutoff.text = " 1e- \t";
    p.communications.source_mode = "random";
    p.communications.manual_bits_text = " 01\nπ🚀\t";
    p.communications.noise_enabled = false;
    p.communications.eb_n0.text = "not a number";
    p.circuits.ac.components[1].phase.text = "disabled but retained";
    p.digital.combinational.inputs[0].name = std::string("a\0b", 3);
    roundtrip(p);
}
TEST(Project, ObjectOrderDoesNotChangeIdentityOrArrayOrder) {
    auto j = document();
    nlohmann::ordered_json reordered = nlohmann::ordered_json::object();
    for (auto it = j.rbegin(); it != j.rend(); ++it)
        reordered[it.key()] = it.value();
    EXPECT_EQ(decode_project(reordered.dump()).snapshot, default_project());
    auto p = default_project();
    std::reverse(p.circuits.dc.nodes.begin(), p.circuits.dc.nodes.end());
    std::reverse(p.digital.combinational.gates.begin(), p.digital.combinational.gates.end());
    roundtrip(p);
    EXPECT_EQ(p.circuits.dc.components[1].positive, Reference{Id{1}});
}
TEST(Project, UnknownFieldsAreReportedAndNotReemitted) {
    auto j = document();
    j["future"] = {{"never_execute", "file:///etc/passwd"}};
    j["signals"]["a/b~c"] = true;
    auto decoded = decode_project(j.dump());
    EXPECT_EQ(decoded.snapshot, default_project());
    EXPECT_NE(std::ranges::find(decoded.ignored_fields, "/future"), decoded.ignored_fields.end());
    EXPECT_NE(std::ranges::find(decoded.ignored_fields, "/signals/a~1b~0c"),
              decoded.ignored_fields.end());
    EXPECT_FALSE(Json::parse(encode_project(decoded.snapshot)).contains("future"));
}
TEST(Project, DuplicateKeysIncludingEscapedKeysAreRejectedBeforeDom) {
    rejected("{\"format\":0,\"format\":1}", ErrorCode::duplicate_key);
    rejected("{\"a\":{\"x\":0,\"\\u0078\":1}}", ErrorCode::duplicate_key);
    rejected("{\"future\":{\"x\":0,\"x\":1}}", ErrorCode::duplicate_key);
}
TEST(Project, RequiredFieldsTypesEnumsAndUnitsAreStrict) {
    auto j = document();
    j["signals"].erase("phase");
    try {
        (void)decode_project(j.dump());
        FAIL();
    } catch (const Error& e) {
        EXPECT_EQ(e.code(), ErrorCode::missing_field);
        EXPECT_EQ(e.path(), "/signals/phase");
    }
    j = document();
    j["signals"]["phase"]["text"] = 0;
    rejected(j.dump(), ErrorCode::wrong_type);
    j = document();
    j["communications"]["noise_enabled"] = 1;
    rejected(j.dump(), ErrorCode::wrong_type);
    j = document();
    j["circuits"]["dc"]["nodes"] = nullptr;
    rejected(j.dump(), ErrorCode::wrong_type);
    j = document();
    j["selected_domain"] = "future";
    rejected(j.dump(), ErrorCode::invalid_value);
    j = document();
    j["circuits"]["ac"]["components"][1]["value"]["unit"] = "V";
    rejected(j.dump(), ErrorCode::invalid_value);
}
TEST(Project, HeaderAndVersionAreNotCoerced) {
    rejected("[]", ErrorCode::wrong_type);
    rejected("{}", ErrorCode::missing_field);
    auto j = document();
    j["format"] = "another app";
    rejected(j.dump(), ErrorCode::wrong_format);
    for (auto version :
         {Json(0), Json(2), Json(-1), Json(std::numeric_limits<std::uint64_t>::max())}) {
        j = document();
        j["schema_version"] = version;
        rejected(j.dump(), ErrorCode::unsupported_version);
    }
    for (auto version : {Json("1"), Json(1.0), Json(true), Json(nullptr)}) {
        j = document();
        j["schema_version"] = version;
        rejected(j.dump(), ErrorCode::wrong_type);
    }
}
TEST(Project, MalformedJsonAndUtf8AreRejected) {
    for (auto bytes :
         {"{", "{} trailing", "{\"x\":NaN}", "{\"x\":1e999}", "{\"x\":1,}", "{/*comment*/}"})
        EXPECT_THROW((void)decode_project(bytes), Error);
    for (auto bytes : {"{\"x\":\"\\uD800\"}", "{\"x\":\"\\uDC00\"}", "{\"x\":\"\xc0\x80\"}"})
        EXPECT_THROW((void)decode_project(bytes), Error);
    auto p = default_project();
    p.signals.phase.text = "\xed\xa0\x80";
    EXPECT_THROW((void)encode_project(p), Error);
}
TEST(Project, IdsAreCanonicalBoundedAndNamesNeverResolveReferences) {
    for (auto value :
         {Json("01"), Json("-1"), Json("4294967296"), Json(""), Json("+1"), Json(" 1")}) {
        auto j = document();
        j["circuits"]["dc"]["ground"] = value;
        rejected(j.dump(), ErrorCode::invalid_identity);
    }
    auto j = document();
    j["circuits"]["dc"]["ground"] = 0;
    rejected(j.dump(), ErrorCode::wrong_type);
    auto p = default_project();
    p.circuits.dc.ground.reset();
    p.circuits.dc.components[0].positive = Id{0xffffffffU};
    p.circuits.dc.nodes[0].name = "4294967295";
    roundtrip(p);
}
TEST(Project, IdentityNamespacesAndCounterInvariants) {
    auto p = default_project();
    p.digital.combinational.gates[0].id = p.digital.combinational.inputs[0].id;
    EXPECT_THROW(validate_structure(p), Error);
    p = default_project();
    p.circuits.dc.components[1].id = p.circuits.dc.components[0].id;
    EXPECT_THROW(validate_structure(p), Error);
    p = default_project();
    p.digital.combinational.next_id = {4};
    EXPECT_THROW(validate_structure(p), Error);
    p = default_project();
    p.circuits.ac.next_node = {limits::exhausted_id + 1};
    EXPECT_THROW(validate_structure(p), Error);
    p = default_project();
    p.circuits.ac.next_node = {limits::exhausted_id};
    roundtrip(p);
    // Node and component ID zero coexist; DC and AC have independent namespaces.
    EXPECT_NO_THROW(validate_structure(default_project()));
}
TEST(Project, AllocationSkipsDeletedDanglingNodesAndSourceReferences) {
    auto p = default_project();
    p.circuits.dc.components[0].positive = Id{3};
    p.circuits.dc.components[0].negative = Id{4};
    p = decode_project(encode_project(p)).snapshot;
    auto refs = reserved_node_ids(p.circuits.dc);
    EXPECT_EQ(allocate_id(p.circuits.dc.next_node, refs), Id{5});
    EXPECT_EQ(p.circuits.dc.next_node.value, 6U);
    p.circuits.ac.reference_source = Id{3};
    auto components = reserved_component_ids(p.circuits.ac);
    EXPECT_EQ(allocate_id(p.circuits.ac.next_component, components), Id{4});
    p.digital.combinational.outputs[0].source = Id{5};
    auto digital = reserved_ids(p.digital.combinational);
    EXPECT_EQ(allocate_id(p.digital.combinational.next_id, digital), Id{6});
}
TEST(Project, TimingReservesDecimalRunsWithoutRewritingInvalidText) {
    auto p = default_project();
    auto& t = p.digital.timing;
    t.elements[0].pins_text = " 004, bad5, ";
    t.outputs[0].source_text = "6";
    t.stimuli[0].input_text = "7";
    t.observed_text = "8,9";
    const auto original = t;
    auto reserved = reserved_ids(t);
    EXPECT_EQ(t, original);
    EXPECT_EQ(allocate_id(t.next_id, reserved), Id{10});
}
TEST(Project, AllocatorExhaustionNeverWraps) {
    NextId next{limits::exhausted_id - 1};
    EXPECT_EQ(allocate_id(next, {}), Id{0xffffffffU});
    EXPECT_EQ(next.value, limits::exhausted_id);
    EXPECT_THROW((void)allocate_id(next, {}), Error);
    next = {limits::exhausted_id - 1};
    const Id reserved{0xffffffffU};
    EXPECT_THROW((void)allocate_id(next, std::span(&reserved, 1)), Error);
    EXPECT_EQ(next.value, limits::exhausted_id);
}
TEST(Project, FieldLimitsHonorUtf8AndUtf16WidgetCapacities) {
    auto p = default_project();
    p.communications.bit_seed_text = std::string(64, 'a');
    roundtrip(p);
    p.communications.bit_seed_text += 'a';
    EXPECT_THROW((void)encode_project(p), Error);
    p = default_project();
    p.circuits.dc.components[0].value.text.clear();
    for (int i = 0; i < 64; ++i)
        p.circuits.dc.components[0].value.text += "🚀";
    roundtrip(p);
    p.circuits.dc.components[0].value.text += "🚀";
    EXPECT_THROW((void)encode_project(p), Error);
    p = default_project();
    p.communications.manual_bits_text = std::string(131072, '1');
    roundtrip(p);
    p.communications.manual_bits_text += '1';
    EXPECT_THROW((void)encode_project(p), Error);
}
TEST(Project, PerDomainArrayLimitsAndEmptyDrafts) {
    ProjectSnapshot empty;
    roundtrip(empty);
    auto p = default_project();
    p.digital.combinational.inputs.resize(limits::digital_inputs + 1);
    EXPECT_THROW((void)encode_project(p), Error);
    auto j = document();
    j["circuits"]["ac"]["nodes"] = Json::array();
    for (std::size_t i = 0; i <= limits::circuit_nodes; ++i)
        j["circuits"]["ac"]["nodes"].push_back({{"id", std::to_string(i)}, {"name", ""}});
    rejected(j.dump(), ErrorCode::resource_limit);
    p = default_project();
    p.digital.timing.stimuli.resize(limits::timing_stimuli);
    roundtrip(p);
    p.digital.timing.stimuli.push_back({});
    EXPECT_THROW((void)encode_project(p), Error);
}
TEST(Project, UntrustedUnknownContentIsBoundedBeforeDom) {
    rejected(std::string(limits::file_bytes + 1, ' '), ErrorCode::resource_limit);
    rejected(std::string(limits::depth + 1, '[') + "0" + std::string(limits::depth + 1, ']'),
             ErrorCode::resource_limit);
    rejected("{\"x\":\"" + std::string(limits::string_bytes + 1, 'x') + "\"}",
             ErrorCode::resource_limit);
    Json members = Json::object();
    for (std::size_t i = 0; i <= limits::members; ++i)
        members[std::to_string(i)] = 0;
    rejected(members.dump(), ErrorCode::resource_limit);
    rejected(Json(std::vector<int>(limits::array_items + 1)).dump(), ErrorCode::resource_limit);
    Json text = Json::array();
    for (int i = 0; i < 9; ++i)
        text.push_back(std::string(limits::string_bytes, 'a'));
    rejected(text.dump(), ErrorCode::resource_limit);
    Json values = Json::array();
    for (int i = 0; i < 21; ++i)
        values.push_back(std::vector<int>(10000));
    rejected(values.dump(), ErrorCode::resource_limit);
}
TEST(Project, EscapedStringsAndKeysObeyDecodedLimits) {
    auto j = document();
    j["circuits"]["dc"]["nodes"][0]["name"] = "π🚀";
    EXPECT_EQ(decode_project(j.dump(-1, ' ', true)).snapshot.circuits.dc.nodes[0].name, "π🚀");
    std::string huge = "{\"x\":\"";
    for (std::size_t i = 0; i <= limits::string_bytes; ++i)
        huge += "\\u0061";
    huge += "\"}";
    rejected(huge, ErrorCode::resource_limit);
    rejected("{\"" + std::string(limits::key_bytes + 1, 'x') + "\":0}", ErrorCode::resource_limit);
}
TEST(Project, DecodeDoesNotModifyExistingSnapshotOnFailure) {
    auto current = default_project();
    const auto saved = current;
    try {
        current = decode_project("{\"schema_version\":9}").snapshot;
    } catch (const Error&) {
    }
    EXPECT_EQ(current, saved);
    auto j = document();
    j["communications"].erase("modulation");
    try {
        current = decode_project(j.dump()).snapshot;
    } catch (const Error&) {
    }
    EXPECT_EQ(current, saved);
}
