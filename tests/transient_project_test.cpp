#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <openece/project/model.hpp>
using namespace openece::project;
using Json = nlohmann::json;
namespace {
ProjectSnapshot draft() {
    auto p = default_project();
    auto& t = p.circuits.transient;
    p.selected_domain = "circuits";
    p.circuits.selected_tab = "transient";
    t.nodes = {{{0}, "Ground π"}, {{4}, "<output>"}};
    t.next_node = {5};
    t.next_component = {8};
    t.ground = Id{99};
    t.components = {{{7},
                     "V 🚀",
                     "voltage_source",
                     Id{4},
                     Id{55},
                     {" 1e-\t", "mV"},
                     {"constant", {{{"", "ms"}, " NaN "}, {{"-2", "s"}, ""}}}}};
    t.initial_conditions = {{Id{90}, "capacitor_voltage", {"?", "uV"}},
                            {std::nullopt, "inductor_current", {"", "mA"}}};
    t.probes = {{"I", "current", Id{123}, Id{456}, Id{89}},
                {"V", "voltage", Id{80}, std::nullopt, Id{88}}};
    t.stop = {" 1e- ", "ns"};
    t.maximum_step = {"", "us"};
    t.initialization = "operating_point";
    t.selected_tab = "sources";
    return p;
}
void reject(const Json& j, ErrorCode code) {
    try {
        decode_project(j.dump());
        FAIL() << "accepted invalid structure";
    } catch (const Error& e) {
        EXPECT_EQ(e.code(), code) << e.what() << e.path();
    }
}
} // namespace
TEST(TransientProject, SchemaTwoPreservesInvalidInactiveOrderedState) {
    auto p = draft();
    auto bytes = encode_project(p);
    auto d = decode_project(bytes);
    EXPECT_EQ(d.source_schema_version, 2);
    EXPECT_EQ(d.snapshot, p);
    EXPECT_EQ(Json::parse(bytes)["schema_version"], 2);
    EXPECT_EQ(decode_project(encode_project(d.snapshot)).snapshot, p);
    auto j = Json::parse(bytes);
    j["circuits"]["transient"]["unknown"] = "ignored";
    auto extra = decode_project(j.dump());
    EXPECT_EQ(extra.snapshot, p);
    EXPECT_EQ(extra.ignored_fields.size(), 1U);
}
TEST(TransientProject, PinnedSchemaOneImportHasExactEmptyDefaultsAndUpgrades) {
    for (auto name : {"complete", "incomplete"}) {
        std::ifstream file(std::string(OPENECE_SOURCE_DIR) + "/tests/fixtures/v0.9/" + name +
                               ".openece",
                           std::ios::binary);
        ASSERT_TRUE(file);
        std::string bytes((std::istreambuf_iterator<char>(file)), {});
        auto d = decode_project(bytes);
        EXPECT_EQ(d.source_schema_version, 1);
        EXPECT_EQ(d.snapshot.circuits.transient, TransientDraft{});
        EXPECT_EQ(d.snapshot.circuits.transient.next_node.value, 0U);
        EXPECT_EQ(d.snapshot.circuits.transient.stop, (Quantity{"", "s"}));
        EXPECT_EQ(decode_project(encode_project(d.snapshot)).snapshot, d.snapshot);
    }
    auto j = Json::parse(encode_project(default_project()));
    j["schema_version"] = 1;
    j["circuits"]["transient"] = "unknown even if malformed";
    auto old = decode_project(j.dump());
    EXPECT_EQ(old.snapshot.circuits.transient, TransientDraft{});
    EXPECT_EQ(old.ignored_fields, (std::vector<std::string>{"/circuits/transient"}));
    j["circuits"]["selected_tab"] = "transient";
    reject(j, ErrorCode::invalid_value);
}
TEST(TransientProject, RequiredTypesTokensIdsAndDuplicateKeys) {
    auto base = Json::parse(encode_project(draft()));
    for (auto key :
         {"next_node", "next_component", "nodes", "components", "ground", "stop", "maximum_step",
          "initialization", "initial_conditions", "probes", "display_time_unit", "selected_tab"}) {
        auto j = base;
        j["circuits"]["transient"].erase(key);
        reject(j, ErrorCode::missing_field);
    }
    auto j = base;
    j["circuits"]["transient"]["components"][0]["source"].erase("points");
    reject(j, ErrorCode::missing_field);
    j = base;
    j["circuits"]["transient"]["probes"][0]["component"] = 12;
    reject(j, ErrorCode::wrong_type);
    j = base;
    j["circuits"]["transient"]["stop"]["unit"] = "Hz";
    reject(j, ErrorCode::invalid_value);
    j = base;
    j["circuits"]["transient"]["initial_conditions"][1]["value"]["unit"] = "V";
    reject(j, ErrorCode::invalid_value);
    j = base;
    j["circuits"]["transient"]["components"][0]["value"]["unit"] = "ohm";
    reject(j, ErrorCode::invalid_value);
    j = base;
    j["circuits"]["transient"]["nodes"][0]["id"] = "00";
    reject(j, ErrorCode::invalid_identity);
    j = base;
    j["schema_version"] = 3;
    reject(j, ErrorCode::unsupported_version);
    EXPECT_THROW(decode_project("{\"transient\":{\"points\":[],\"points\":[]}}"), Error);
}
TEST(TransientProject, AllocationReservesInactiveDanglingWithoutCounterJump) {
    auto p = draft();
    auto& t = p.circuits.transient;
    t.probes[0].positive = Id{5};
    t.probes[0].negative = Id{6};
    t.probes[0].component = Id{8};
    t.initial_conditions[0].component = Id{9};
    p = decode_project(encode_project(p)).snapshot;
    EXPECT_EQ(p.circuits.transient.next_node.value, 5U); // not > dangling 99 or 456
    EXPECT_EQ(allocate_id(t.next_node, reserved_node_ids(t)), Id{7});
    EXPECT_EQ(allocate_id(t.next_component, reserved_component_ids(t)), Id{10});
    t.next_node = {limits::exhausted_id};
    EXPECT_THROW(allocate_id(t.next_node, reserved_node_ids(t)), Error);
    EXPECT_NO_THROW(validate_structure(p));
    t.next_component = {7};
    EXPECT_THROW(validate_structure(p), Error); // declared identity
}
TEST(TransientProject, LimitsCountDisabledSourcesAndAllRows) {
    auto p = draft();
    auto& t = p.circuits.transient;
    t.components[0].source.points.resize(limits::transient_source_points);
    EXPECT_NO_THROW(encode_project(p));
    t.components[0].source.points.push_back({});
    EXPECT_THROW(encode_project(p), Error);
    t.components[0].source.points.resize(limits::transient_source_points);
    for (unsigned i = 0; i < 3; ++i) {
        auto c = t.components[0];
        c.id = {10 + i};
        t.components.push_back(c);
    }
    t.next_component = {13};
    EXPECT_NO_THROW(encode_project(p));
    auto c = t.components[0];
    c.id = {13};
    c.source.points.resize(1);
    t.components.push_back(c);
    t.next_component = {14};
    EXPECT_THROW(encode_project(p), Error);
    p = draft();
    t.probes.resize(limits::transient_probes);
    EXPECT_NO_THROW(encode_project(p));
    t.probes.push_back({});
    EXPECT_THROW(encode_project(p), Error);
    p = draft();
    t.initial_conditions.resize(limits::transient_initial_conditions);
    EXPECT_NO_THROW(encode_project(p));
    t.initial_conditions.push_back({});
    EXPECT_THROW(encode_project(p), Error);
    p = draft();
    t.stop.text = std::string(129, 'a');
    EXPECT_THROW(encode_project(p), Error);
    p = draft();
    t.components[0].source.points[0].value_text = std::string(129, 'a');
    EXPECT_THROW(encode_project(p), Error);
    p = draft();
    p.digital.timing.stimuli.resize(10000);
    t.components[0].source.points.resize(1024);
    for (unsigned i = 0; i < 2; ++i) {
        auto c = t.components[0];
        c.id = {10 + i};
        t.components.push_back(c);
    }
    t.next_component = {12};
    EXPECT_THROW(encode_project(p), Error); // global 12000 rows, not just per-domain counts
}
