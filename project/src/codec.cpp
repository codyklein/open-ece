#include "fields.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>
#include <unordered_set>
namespace openece::project {
namespace {
using Json = nlohmann::json;
using detail::Rule;
[[noreturn]] void fail(ErrorCode code, const std::string& path, const char* message) {
    throw Error(code, path, message);
}
std::string child(const std::string& path, std::string_view key) {
    std::string result = path + "/";
    for (char c : key) {
        if (c == '~')
            result += "~0";
        else if (c == '/')
            result += "~1";
        else
            result += c;
    }
    return result;
}
std::size_t utf16_length(std::string_view s, const std::string& path) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < s.size();) {
        const auto b = static_cast<unsigned char>(s[i++]);
        std::uint32_t cp = b;
        unsigned rest = 0;
        std::uint32_t minimum = 0;
        if (b < 0x80) {
        } else if (b >= 0xc2 && b <= 0xdf) {
            cp = b & 31U;
            rest = 1;
            minimum = 0x80;
        } else if (b >= 0xe0 && b <= 0xef) {
            cp = b & 15U;
            rest = 2;
            minimum = 0x800;
        } else if (b >= 0xf0 && b <= 0xf4) {
            cp = b & 7U;
            rest = 3;
            minimum = 0x10000;
        } else
            fail(ErrorCode::invalid_utf8, path, "Invalid UTF-8 leading byte");
        while (rest--) {
            if (i == s.size())
                fail(ErrorCode::invalid_utf8, path, "Truncated UTF-8 sequence");
            auto c = static_cast<unsigned char>(s[i++]);
            if ((c & 0xc0) != 0x80)
                fail(ErrorCode::invalid_utf8, path, "Invalid UTF-8 continuation");
            cp = (cp << 6) | (c & 63U);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            fail(ErrorCode::invalid_utf8, path, "Invalid Unicode scalar");
        count += cp > 0xffff ? 2 : 1;
    }
    return count;
}
bool token(std::string_view value, std::string_view allowed) {
    while (!allowed.empty()) {
        auto n = allowed.find('|');
        if (value == allowed.substr(0, n))
            return true;
        if (n == std::string_view::npos)
            break;
        allowed.remove_prefix(n + 1);
    }
    return false;
}
void check_text(std::string_view s, Rule rule, const std::string& path) {
    if (s.size() > rule.bytes || utf16_length(s, path) > rule.utf16)
        fail(ErrorCode::resource_limit, path, "Text exceeds field capacity");
    if (!rule.tokens.empty() && !token(s, rule.tokens))
        fail(ErrorCode::invalid_value, path, "Unsupported enum or unit token");
}
std::uint64_t decimal(const Json& j, std::uint64_t maximum, const std::string& path) {
    if (!j.is_string())
        fail(ErrorCode::wrong_type, path, "ID/counter must be a decimal string");
    const auto& s = j.get_ref<const std::string&>();
    if (s.empty() || s.size() > 10 || (s.size() > 1 && s[0] == '0') ||
        s.find_first_not_of("0123456789") != std::string::npos)
        fail(ErrorCode::invalid_identity, path, "ID/counter must use canonical decimal digits");
    std::uint64_t value = 0;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc{} || value > maximum)
        fail(ErrorCode::invalid_identity, path, "ID/counter out of range");
    return value;
}
template <class T> struct is_vector : std::false_type {};
template <class T> struct is_vector<std::vector<T>> : std::true_type {};
struct Budget {
    std::size_t text = 0, rows = 0;
};
struct Validator;
template <class T> void validate(const T&, Rule, const std::string&, Budget&);
struct Validator {
    std::string path;
    Budget& budget;
    template <class T> void field(const char* name, const T& v, Rule rule = {}) {
        validate(v, rule, child(path, name), budget);
    }
};
template <class T> void validate(const T& v, Rule rule, const std::string& path, Budget& budget) {
    if constexpr (std::is_same_v<T, std::string>) {
        check_text(v, rule, path);
        if (v.size() > limits::text_bytes - budget.text)
            fail(ErrorCode::resource_limit, path, "Aggregate text limit exceeded");
        budget.text += v.size();
    } else if constexpr (std::is_same_v<T, NextId>) {
        if (v.value > limits::exhausted_id)
            fail(ErrorCode::invalid_allocator_state, path,
                 "ID counter exceeds exhaustion sentinel");
    } else if constexpr (std::is_same_v<T, Id> || std::is_same_v<T, Reference> ||
                         std::is_same_v<T, bool>) {
    } else if constexpr (is_vector<T>::value) {
        if (v.size() > rule.count || v.size() > limits::rows - budget.rows)
            fail(ErrorCode::resource_limit, path, "Array or aggregate row limit exceeded");
        budget.rows += v.size();
        for (std::size_t i = 0; i < v.size(); ++i)
            validate(v[i], {}, child(path, std::to_string(i)), budget);
    } else {
        Validator a{path, budget};
        detail::fields(a, v, rule);
    }
}
struct Reader;
template <class T> T read(const Json&, Rule, const std::string&, std::vector<std::string>&);
struct Reader {
    const Json& object;
    std::string path;
    std::vector<std::string>& warnings;
    std::set<std::string> seen;
    template <class T> void field(const char* name, T& value, Rule rule = {}) {
        seen.insert(name);
        const auto p = child(path, name);
        auto it = object.find(name);
        if (it == object.end())
            fail(ErrorCode::missing_field, p, "Required field is missing");
        value = read<T>(*it, rule, p, warnings);
    }
    void finish() {
        for (auto it = object.begin(); it != object.end(); ++it)
            if (!seen.contains(it.key()))
                warnings.push_back(child(path, it.key()));
    }
};
template <class T>
T read(const Json& j, Rule rule, const std::string& path, std::vector<std::string>& warnings) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (!j.is_string())
            fail(ErrorCode::wrong_type, path, "Expected a string");
        const auto& s = j.get_ref<const std::string&>();
        check_text(s, rule, path);
        return s;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!j.is_boolean())
            fail(ErrorCode::wrong_type, path, "Expected a Boolean");
        return j.get<bool>();
    } else if constexpr (std::is_same_v<T, Id>)
        return {static_cast<std::uint32_t>(decimal(j, limits::exhausted_id - 1, path))};
    else if constexpr (std::is_same_v<T, NextId>)
        return {decimal(j, limits::exhausted_id, path)};
    else if constexpr (std::is_same_v<T, Reference>) {
        if (j.is_null())
            return std::nullopt;
        return read<Id>(j, {}, path, warnings);
    } else if constexpr (is_vector<T>::value) {
        if (!j.is_array())
            fail(ErrorCode::wrong_type, path, "Expected an array");
        if (j.size() > rule.count)
            fail(ErrorCode::resource_limit, path, "Array exceeds field limit");
        T values;
        values.reserve(j.size());
        for (std::size_t i = 0; i < j.size(); ++i)
            values.push_back(
                read<typename T::value_type>(j[i], {}, child(path, std::to_string(i)), warnings));
        return values;
    } else {
        if (!j.is_object())
            fail(ErrorCode::wrong_type, path, "Expected an object");
        T value;
        Reader r{j, path, warnings, {}};
        detail::fields(r, value, rule);
        r.finish();
        return value;
    }
}
struct Writer;
template <class T> Json write(const T&, Rule = {});
struct Writer {
    Json value = Json::object();
    template <class T> void field(const char* name, const T& v, Rule rule = {}) {
        value[name] = write(v, rule);
    }
};
template <class T> Json write(const T& v, Rule rule) {
    if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>)
        return v;
    else if constexpr (std::is_same_v<T, Id> || std::is_same_v<T, NextId>)
        return std::to_string(v.value);
    else if constexpr (std::is_same_v<T, Reference>)
        return v ? write(*v) : Json(nullptr);
    else if constexpr (is_vector<T>::value) {
        auto j = Json::array();
        for (auto& item : v)
            j.push_back(write(item));
        return j;
    } else {
        Writer a;
        detail::fields(a, v, rule);
        return a.value;
    }
}
// Preflight bounds the lexer's string and numeric buffers before invoking the library.
// Syntax itself is still validated by nlohmann's strict parser, not this scanner.
void preflight(std::string_view s) {
    if (s.size() > limits::file_bytes)
        fail(ErrorCode::resource_limit, "", "Project exceeds 8 MiB");
    (void)utf16_length(s, "");
    std::size_t total = 0;
    auto hex = [&](std::size_t& i) {
        unsigned cp = 0;
        for (int k = 0; k < 4; ++k) {
            if (i == s.size())
                fail(ErrorCode::malformed_json, "", "Truncated Unicode escape");
            char c = s[i++];
            unsigned digit;
            if (c >= '0' && c <= '9')
                digit = static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')
                digit = static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                digit = static_cast<unsigned>(c - 'A' + 10);
            else
                fail(ErrorCode::malformed_json, "", "Invalid Unicode escape");
            cp = (cp << 4) | digit;
        }
        return cp;
    };
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == '"') {
            ++i;
            std::size_t bytes = 0;
            bool closed = false;
            while (i < s.size()) {
                char c = s[i++];
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\\') {
                    if (i == s.size())
                        fail(ErrorCode::malformed_json, "", "Truncated escape");
                    char escape = s[i++];
                    if (escape == 'u') {
                        unsigned cp = hex(i);
                        if (cp >= 0xd800 && cp <= 0xdbff) {
                            if (i + 2 > s.size() || s[i] != '\\' || s[i + 1] != 'u')
                                fail(ErrorCode::invalid_utf8, "", "Unpaired surrogate");
                            i += 2;
                            auto low = hex(i);
                            if (low < 0xdc00 || low > 0xdfff)
                                fail(ErrorCode::invalid_utf8, "", "Unpaired surrogate");
                            bytes += 4;
                        } else {
                            if (cp >= 0xdc00 && cp <= 0xdfff)
                                fail(ErrorCode::invalid_utf8, "", "Unpaired surrogate");
                            bytes += cp < 0x80 ? 1 : cp < 0x800 ? 2 : 3;
                        }
                    } else {
                        if (std::string_view("\"\\/bfnrt").find(escape) == std::string_view::npos)
                            fail(ErrorCode::malformed_json, "", "Invalid escape");
                        ++bytes;
                    }
                } else
                    ++bytes;
                if (bytes > limits::string_bytes)
                    fail(ErrorCode::resource_limit, "", "JSON string too large");
            }
            if (!closed)
                fail(ErrorCode::malformed_json, "", "Unterminated string");
            std::size_t next = i;
            while (next < s.size() &&
                   std::string_view(" \r\n\t").find(s[next]) != std::string_view::npos)
                ++next;
            if (next < s.size() && s[next] == ':' && bytes > limits::key_bytes)
                fail(ErrorCode::resource_limit, "", "JSON key too large");
            if (bytes > limits::text_bytes - total)
                fail(ErrorCode::resource_limit, "", "Aggregate JSON text too large");
            total += bytes;
        } else if ((s[i] >= '0' && s[i] <= '9') || s[i] == '-') {
            auto start = i++;
            while (i < s.size() &&
                   std::string_view("0123456789.eE+-").find(s[i]) != std::string_view::npos)
                ++i;
            if (i - start > 64)
                fail(ErrorCode::resource_limit, "", "JSON number token too long");
        } else
            ++i;
    }
}
struct Accounting : nlohmann::json_sax<Json> {
    struct Level {
        bool object;
        std::size_t count = 0;
        std::set<std::string> keys;
    };
    std::vector<Level> stack;
    std::size_t values = 0;
    bool value() {
        if (++values > limits::values)
            fail(ErrorCode::resource_limit, "", "Too many JSON values");
        if (!stack.empty() && !stack.back().object && ++stack.back().count > limits::array_items)
            fail(ErrorCode::resource_limit, "", "JSON array too large");
        return true;
    }
    bool start(bool object) {
        value();
        if (stack.size() >= limits::depth)
            fail(ErrorCode::resource_limit, "", "JSON nesting too deep");
        stack.push_back({object, 0, {}});
        return true;
    }
    bool null() override { return value(); }
    bool boolean(bool) override { return value(); }
    bool number_integer(number_integer_t) override { return value(); }
    bool number_unsigned(number_unsigned_t) override { return value(); }
    bool number_float(number_float_t n, const string_t&) override {
        if (!std::isfinite(n))
            fail(ErrorCode::invalid_value, "", "Nonfinite JSON number");
        return value();
    }
    bool string(string_t&) override { return value(); }
    bool binary(binary_t&) override { return false; }
    bool start_object(std::size_t) override { return start(true); }
    bool key(string_t& key) override {
        auto& level = stack.back();
        if (++level.count > limits::members)
            fail(ErrorCode::resource_limit, "", "Too many object members");
        if (!level.keys.insert(key).second)
            fail(ErrorCode::duplicate_key, "", "Duplicate JSON object key");
        return true;
    }
    bool end_object() override {
        stack.pop_back();
        return true;
    }
    bool start_array(std::size_t) override { return start(false); }
    bool end_array() override {
        stack.pop_back();
        return true;
    }
    bool parse_error(std::size_t position, const std::string&,
                     const nlohmann::detail::exception&) override {
        throw Error(ErrorCode::malformed_json, "", "Malformed JSON or invalid UTF-8", position);
    }
};
template <class Rows>
void identities(const Rows& rows, std::unordered_set<std::uint32_t>& seen, const std::string& path,
                bool nonzero = false) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto id = rows[i].id.value;
        auto p = child(child(path, std::to_string(i)), "id");
        if (nonzero && id == 0)
            fail(ErrorCode::invalid_identity, p,
                 "Zero is reserved for unconnected combinational references");
        if (!seen.insert(id).second)
            fail(ErrorCode::duplicate_identity, p, "Duplicate identity in namespace");
    }
}
void counter(NextId next, const std::unordered_set<std::uint32_t>& ids, const std::string& path,
             bool nonzero = false) {
    if ((nonzero && next.value == 0) ||
        std::ranges::any_of(ids, [&](auto id) { return next.value <= id; }))
        fail(ErrorCode::invalid_allocator_state, path, "Counter must exceed all declared IDs");
}
} // namespace
void validate_structure(const ProjectSnapshot& p) {
    Budget b;
    validate(p, {}, "", b);
    std::unordered_set<std::uint32_t> ids;
    auto& c = p.digital.combinational;
    identities(c.inputs, ids, "/digital/combinational/inputs", true);
    identities(c.gates, ids, "/digital/combinational/gates", true);
    counter(c.next_id, ids, "/digital/combinational/next_id", true);
    for (auto& gate : c.gates)
        for (auto pin : gate.pins)
            if (pin && pin->value == 0)
                fail(ErrorCode::invalid_identity, "/digital/combinational/gates",
                     "Use null for an unconnected pin");
    for (auto& output : c.outputs)
        if (output.source && output.source->value == 0)
            fail(ErrorCode::invalid_identity, "/digital/combinational/outputs",
                 "Use null for an unconnected output");
    ids.clear();
    auto& t = p.digital.timing;
    identities(t.inputs, ids, "/digital/timing/inputs");
    identities(t.elements, ids, "/digital/timing/elements");
    counter(t.next_id, ids, "/digital/timing/next_id");
    auto circuit = [&](const auto& draft, const std::string& path) {
        ids.clear();
        identities(draft.nodes, ids, path + "/nodes");
        counter(draft.next_node, ids, path + "/next_node");
        ids.clear();
        identities(draft.components, ids, path + "/components");
        counter(draft.next_component, ids, path + "/next_component");
    };
    circuit(p.circuits.dc, "/circuits/dc");
    circuit(p.circuits.ac, "/circuits/ac");
}
DecodedProject decode_project(std::string_view utf8) {
    preflight(utf8);
    Accounting sax;
    if (!Json::sax_parse(utf8, &sax))
        fail(ErrorCode::malformed_json, "", "Invalid JSON");
    auto root = Json::parse(utf8);
    if (!root.is_object())
        fail(ErrorCode::wrong_type, "", "Project root must be an object");
    auto required = [&](const char* name) -> const Json& {
        auto it = root.find(name);
        if (it == root.end())
            fail(ErrorCode::missing_field, child("", name), "Missing project header");
        return *it;
    };
    const auto& format = required("format");
    if (!format.is_string())
        fail(ErrorCode::wrong_type, "/format", "Format must be a string");
    if (format != "org.openece.project")
        fail(ErrorCode::wrong_format, "/format", "Not an OpenECE project");
    const auto& version = required("schema_version");
    if (!version.is_number_integer())
        fail(ErrorCode::wrong_type, "/schema_version", "Schema version must be an integer");
    if (version != 1)
        fail(ErrorCode::unsupported_version, "/schema_version",
             "Only OpenECE project schema 1 is supported");
    DecodedProject result;
    Reader r{root, "", result.ignored_fields, {"format", "schema_version"}};
    detail::fields(r, result.snapshot);
    r.finish();
    validate_structure(result.snapshot);
    return result;
}
std::string encode_project(const ProjectSnapshot& snapshot) {
    validate_structure(snapshot);
    auto root = write(snapshot);
    root["format"] = "org.openece.project";
    root["schema_version"] = 1;
    auto result = root.dump(2) + '\n';
    if (result.size() > limits::file_bytes)
        fail(ErrorCode::resource_limit, "", "Encoded project exceeds 8 MiB");
    preflight(result);
    Accounting accounting;
    (void)Json::sax_parse(result, &accounting);
    return result;
}
} // namespace openece::project
