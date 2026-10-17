#include "modbridge/nexus.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <utility>
#include <vector>

namespace modbridge {
namespace {
constexpr std::size_t max_body_bytes = 8U * 1024U * 1024U;
constexpr std::size_t max_depth = 64;
constexpr std::size_t max_nodes = 200000;
constexpr std::size_t max_array_items = 10000;
constexpr std::size_t max_download_uri_bytes = 8192;
constexpr auto max_identifier = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
enum class Failure { Malformed, ResourceLimit };

Json envelope(const char* status) {
    return {{"schema_version", record_schema_version}, {"status", status},
            {"scope", "nexus_metadata_only"}, {"installation_supported", false},
            {"issues", Json::array()}};
}

Json error_envelope(const char* status, const char* code) {
    auto result = envelope(status);
    result["issues"].push_back({{"code", code}});
    return result;
}

std::size_t text_characters(std::string_view text) {
    // The JSON lexer already validates UTF-8. Counting leading bytes matches
    // Python's Unicode code-point limit without a locale or normalization rule.
    std::size_t count = 0;
    for (unsigned char byte : text) {
        if (byte < 32 && byte != '\n' && byte != '\r' && byte != '\t')
            throw Failure::Malformed;
        if ((byte & 0xc0U) != 0x80U) ++count;
    }
    return count;
}

struct ParseFrame {
    bool object;
    std::size_t array_items = 0;
    std::set<std::string> keys;
};

class ParseGate {
public:
    bool event(Json::parse_event_t event, Json& value) {
        using Event = Json::parse_event_t;
        switch (event) {
        case Event::object_start:
        case Event::array_start:
            add_value();
            // Root counts as one container. Check before the DOM parser adds
            // the container; the callback is invoked before its handle_value.
            if (frames_.size() >= max_depth) throw Failure::ResourceLimit;
            frames_.push_back({event == Event::object_start, 0, {}});
            break;
        case Event::key: {
            add_node();
            const auto& key = value.get_ref<const std::string&>();
            (void)text_characters(key);
            if (!frames_.back().keys.insert(key).second) throw Failure::Malformed;
            break;
        }
        case Event::value:
            add_value();
            if (value.is_string()) (void)text_characters(value.get_ref<const std::string&>());
            break;
        case Event::object_end:
        case Event::array_end:
            frames_.pop_back();
            break;
        }
        return true;
    }

private:
    void add_node() {
        if (nodes_ >= max_nodes) throw Failure::ResourceLimit;
        ++nodes_;
    }
    void add_value() {
        add_node();
        if (!frames_.empty() && !frames_.back().object) {
            if (frames_.back().array_items >= max_array_items) throw Failure::ResourceLimit;
            ++frames_.back().array_items;
        }
    }
    std::size_t nodes_ = 0;
    std::vector<ParseFrame> frames_;
};

const Json& member(const Json& object, const char* key) {
    if (!object.is_object()) throw Failure::Malformed;
    const auto found = object.find(key);
    if (found == object.end()) throw Failure::Malformed;
    return *found;
}

const Json& array(const Json& value) {
    if (!value.is_array()) throw Failure::Malformed;
    return value;
}

const std::string& text(const Json& value, std::size_t maximum) {
    if (!value.is_string()) throw Failure::Malformed;
    const auto& result = value.get_ref<const std::string&>();
    if (text_characters(result) > maximum) throw Failure::Malformed;
    return result;
}

std::string optional_text(const Json& object, const char* key, std::size_t maximum) {
    const auto found = object.find(key);
    return found == object.end() ? std::string{} : text(*found, maximum);
}

std::uint64_t nonnegative_integer(const Json& value) {
    if (value.is_number_unsigned()) return value.get<std::uint64_t>();
    if (value.type() != Json::value_t::number_integer) throw Failure::Malformed;
    const auto result = value.get<std::int64_t>();
    if (result < 0) throw Failure::Malformed;
    return static_cast<std::uint64_t>(result);
}

std::uint64_t identifier(const Json& value) {
    const auto result = nonnegative_integer(value);
    if (!result || result > max_identifier) throw Failure::Malformed;
    return result;
}

bool domain_valid(std::string_view domain) {
    const auto alphanumeric = [](unsigned char byte) {
        return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
    };
    if (domain.empty() || domain.size() > 80 || !alphanumeric(static_cast<unsigned char>(domain.front())))
        return false;
    for (unsigned char byte : domain)
        if (!alphanumeric(byte) && byte != '_' && byte != '-') return false;
    return true;
}

void clear_bytes(void* data, std::size_t size) noexcept {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    while (size--) *bytes++ = 0;
}

bool valid_range(const void* data, std::size_t size) noexcept {
    return !size || (data && reinterpret_cast<std::uintptr_t>(data)
                                  <= std::numeric_limits<std::uintptr_t>::max() - size);
}

bool overlaps(const void* a, std::size_t an, const void* b, std::size_t bn) noexcept {
    if (!an || !bn) return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a);
    const auto y = reinterpret_cast<std::uintptr_t>(b);
    return x < y + bn && y < x + an;
}

bool unreserved(unsigned char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}

std::size_t decimal_size(std::uint64_t value) noexcept {
    std::size_t size = 1;
    while (value >= 10) { value /= 10; ++size; }
    return size;
}

void write_decimal(char*& destination, std::uint64_t value) noexcept {
    const auto size = decimal_size(value);
    for (std::size_t i = size; i > 0; --i) {
        destination[i - 1] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    destination += size;
}

void scrub_json_strings(Json& value) noexcept {
    if (value.is_string()) {
        auto& text = value.get_ref<std::string&>();
        clear_bytes(text.data(), text.size());
    } else if (value.is_structured()) {
        for (auto& child : value) scrub_json_strings(child);
    }
}

struct ParsedStringCleanup {
    Json& value;
    ~ParsedStringCleanup() { scrub_json_strings(value); }
};

bool ipv4_tail(std::string_view host) noexcept {
    unsigned fields = 0;
    std::size_t pos = 0;
    while (pos < host.size()) {
        unsigned value = 0, digits = 0;
        while (pos < host.size() && host[pos] >= '0' && host[pos] <= '9') {
            value = value * 10 + static_cast<unsigned>(host[pos++] - '0');
            if (++digits > 3 || value > 255) return false;
        }
        if (!digits || ++fields > 4) return false;
        if (pos == host.size()) return fields == 4;
        if (host[pos++] != '.' || pos == host.size()) return false;
    }
    return false;
}

bool ipv6_host(std::string_view host) noexcept {
    unsigned groups = 0;
    bool compressed = false;
    std::size_t pos = 0;
    if (host.empty()) return false;
    if (host.front() == ':') {
        if (host.size() < 2 || host[1] != ':') return false;
        compressed = true; pos = 2;
    }
    while (pos < host.size()) {
        const auto begin = pos;
        while (pos < host.size() && host[pos] != ':') ++pos;
        const auto group = host.substr(begin, pos - begin);
        if (group.find('.') != std::string_view::npos) {
            if (pos != host.size() || !ipv4_tail(group)) return false;
            groups += 2;
        } else {
            if (group.empty() || group.size() > 4) return false;
            for (unsigned char c : group)
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                      (c >= 'A' && c <= 'F'))) return false;
            ++groups;
        }
        if (groups > 8) return false;
        if (pos == host.size()) break;
        ++pos;
        if (pos < host.size() && host[pos] == ':') {
            if (compressed) return false;
            compressed = true; ++pos;
        } else if (pos == host.size()) return false;
    }
    return compressed ? groups < 8 : groups == 8;
}

bool oauth_redirect_valid(std::string_view uri) noexcept {
    if (uri.empty() || uri.size() > 4096) return false;
    for (unsigned char c : uri)
        if (c < 33 || c > 126 || c == '\\' || c == '?' || c == '#' || c == '@') return false;
    std::size_t start = 0;
    if (uri.starts_with("http://")) start = 7;
    else if (uri.starts_with("https://")) start = 8;
    else return false;
    const auto slash = uri.find('/', start);
    if (slash == std::string_view::npos || uri.substr(slash) != "/auth/callback") return false;
    const auto authority = uri.substr(start, slash - start);
    if (authority.empty()) return false;
    std::string_view host, port;
    bool has_port = false;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos || !ipv6_host(authority.substr(1, close - 1))) return false;
        host = authority.substr(0, close + 1);
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') return false;
            port = authority.substr(close + 2); has_port = true;
        }
    } else {
        const auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != std::string_view::npos) { port = authority.substr(colon + 1); has_port = true; }
        if (host.empty() || host.size() > 253) return false;
        for (unsigned char c : host)
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
    }
    if (has_port) {
        unsigned value = 0;
        if (port.empty() || port.size() > 5) return false;
        for (unsigned char c : port) {
            if (c < '0' || c > '9') return false;
            value = value * 10 + static_cast<unsigned>(c - '0');
        }
        if (!value || value > 65535) return false;
    }
    return true;
}

bool oauth_fields_valid(std::string_view client, std::string_view redirect,
                        std::string_view code, std::string_view verifier) noexcept {
    if (client.empty() || client.size() > 128 || client == "public_test" ||
        code.empty() || code.size() > 2048 || verifier.size() < 43 || verifier.size() > 128 ||
        !oauth_redirect_valid(redirect)) return false;
    for (unsigned char c : client)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    for (unsigned char c : code) if (c < 33 || c > 126) return false;
    for (unsigned char c : verifier) if (!unreserved(c)) return false;
    return true;
}

bool download_uri_valid(std::string_view uri) noexcept {
    constexpr std::string_view scheme = "https://";
    if (uri.size() <= scheme.size() || uri.size() >= max_download_uri_bytes) return false;
    for (unsigned char c : uri)
        if (c < 33 || c > 126 || c == '\\' || c == '#') return false;
    for (std::size_t i = 0; i < scheme.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(uri[i]);
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != scheme[i]) return false;
    }
    const auto end = uri.find_first_of("/?", scheme.size());
    const auto authority = uri.substr(scheme.size(), end == std::string_view::npos
                                                       ? end : end - scheme.size());
    if (authority.empty() || authority.find('@') != std::string_view::npos) return false;
    std::string_view host, port;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == std::string_view::npos || !ipv6_host(authority.substr(1, close - 1))) return false;
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') return false;
            port = authority.substr(close + 2);
            if (port.empty()) return false;
        }
    } else {
        const auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (host.empty()) return false;
        for (unsigned char c : host)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_')) return false;
        if (colon != std::string_view::npos) {
            port = authority.substr(colon + 1);
            if (port.empty()) return false;
        }
    }
    if (!port.empty()) {
        if (port.size() > 5) return false;
        unsigned value = 0;
        for (unsigned char c : port) {
            if (c < '0' || c > '9') return false;
            value = value * 10 + c - '0';
        }
        if (value != 443) return false;
    }
    return true;
}

void expected_identity(std::string_view domain, std::uint64_t mod_id, bool mod_required) {
    if (!domain_valid(domain) || mod_id > max_identifier || (mod_required && !mod_id))
        throw Failure::Malformed;
}

bool boolean(const Json& value) {
    if (!value.is_boolean()) throw Failure::Malformed;
    return value.get<bool>();
}

Json optional_boolean(const Json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_boolean() ? *found : Json(nullptr);
}

std::string game_url(std::string_view domain) {
    return "https://www.nexusmods.com/" + std::string(domain);
}

std::string mod_url(std::string_view domain, std::uint64_t mod_id) {
    return game_url(domain) + "/mods/" + std::to_string(mod_id);
}

Json game_record(const Json& input) {
    const auto id = identifier(member(input, "id"));
    const auto& domain = text(member(input, "domain_name"), 80);
    if (!domain_valid(domain)) throw Failure::Malformed;
    Json result = {{"id", id}, {"domain_name", domain},
                   {"name", text(member(input, "name"), 256)}, {"source_url", game_url(domain)}};
    const auto count = input.find("mods");
    if (count != input.end()) result["mods"] = nonnegative_integer(*count);
    return result;
}

Json mod_record(const Json& input, std::string_view domain, std::uint64_t expected_mod_id) {
    const auto mod_id = identifier(member(input, "mod_id"));
    const auto game_id = identifier(member(input, "game_id"));
    if (text(member(input, "domain_name"), 80) != domain || (expected_mod_id && mod_id != expected_mod_id))
        throw Failure::Malformed;
    return {{"mod_id", mod_id}, {"game_id", game_id}, {"game_domain", std::string(domain)},
            {"name", text(member(input, "name"), 1024)}, {"author", text(member(input, "author"), 1024)},
            {"summary", optional_text(input, "summary", 65536)},
            {"description", optional_text(input, "description", 65536)},
            {"version", optional_text(input, "version", 256)},
            {"contains_adult_content", optional_boolean(input, "contains_adult_content")},
            {"available", boolean(member(input, "available"))}, {"status", text(member(input, "status"), 256)},
            {"source_url", mod_url(domain, mod_id)}};
}

Json file_record(const Json& input, std::string_view domain, std::uint64_t mod_id) {
    const auto id = identifier(member(input, "file_id"));
    const auto& size = member(input, "size_kb");
    if (!size.is_number()) throw Failure::Malformed;
    const auto numeric_size = size.get<double>();
    if (!std::isfinite(numeric_size) || numeric_size < 0 || numeric_size > 1e12)
        throw Failure::Malformed;
    return {{"file_id", id}, {"name", text(member(input, "name"), 1024)},
            {"file_name", text(member(input, "file_name"), 1024)},
            {"version", text(member(input, "version"), 256)},
            {"description", optional_text(input, "description", 65536)}, {"size_kb", size},
            {"source_url", mod_url(domain, mod_id) + "?tab=files&file_id=" + std::to_string(id)}};
}

Json oauth_profile(const Json& input) {
    const auto& subject = text(member(input, "sub"), 19);
    if (subject.empty()) throw Failure::Malformed;
    std::uint64_t id = 0;
    for (unsigned char byte : subject) {
        if (byte < '0' || byte > '9') throw Failure::Malformed;
        const auto digit = static_cast<std::uint64_t>(byte - '0');
        if (id > (max_identifier - digit) / 10) throw Failure::Malformed;
        id = id * 10 + digit;
    }
    if (!id) throw Failure::Malformed;
    const auto& roles = array(member(input, "membership_roles"));
    if (roles.size() > 100) throw Failure::Malformed;
    bool premium = false, supporter = false;
    for (const auto& role : roles) {
        const auto& name = text(role, 128);
        premium = premium || name == "premium";
        supporter = supporter || name == "supporter";
    }
    Json adult = nullptr;
    const auto preferences = input.find("preferences");
    if (preferences != input.end() && preferences->is_object())
        adult = optional_boolean(*preferences, "adult");
    return {{"user_id", id}, {"display_name", text(member(input, "name"), 256)},
            {"is_premium", premium}, {"is_supporter", supporter}, {"adult_content", std::move(adult)}};
}

Json api_key_profile(const Json& input) {
    return {{"user_id", identifier(member(input, "user_id"))},
            {"display_name", text(member(input, "name"), 256)},
            {"is_premium", boolean(member(input, "is_premium"))},
            {"is_supporter", boolean(member(input, "is_supporter"))}, {"adult_content", nullptr}};
}
} // namespace

std::optional<std::string> nexus_metadata_url(NexusRecordKind kind, std::string_view domain, std::uint64_t mod_id) {
    constexpr std::string_view api = "https://api.nexusmods.com/v1";
    switch (kind) {
    case NexusRecordKind::Games:
    case NexusRecordKind::OAuthUser:
    case NexusRecordKind::ApiKeyUser:
        if (!domain.empty() || mod_id) return std::nullopt;
        if (kind == NexusRecordKind::Games) return std::string(api) + "/games";
        if (kind == NexusRecordKind::ApiKeyUser) return std::string(api) + "/users/validate";
        return std::string("https://users.nexusmods.com/oauth/userinfo");
    case NexusRecordKind::Mods:
        if (!domain_valid(domain) || mod_id) return std::nullopt;
        return std::string(api) + "/games/" + std::string(domain) + "/mods/latest_added";
    case NexusRecordKind::Mod:
    case NexusRecordKind::Files:
        if (!domain_valid(domain) || !mod_id || mod_id > max_identifier) return std::nullopt;
        return std::string(api) + "/games/" + std::string(domain) + "/mods/" + std::to_string(mod_id)
             + (kind == NexusRecordKind::Files ? "/files" : "");
    default:
        return std::nullopt;
    }
}

bool nexus_download_link_url(std::string_view domain, std::uint64_t mod_id,
    std::uint64_t file_id, std::string_view nxm_key, std::uint64_t expires,
    std::span<char> output, std::size_t& written) noexcept {
    if (output.size() > max_download_uri_bytes ||
        !valid_range(output.data(), output.size()) ||
        !valid_range(domain.data(), domain.size()) ||
        !valid_range(nxm_key.data(), nxm_key.size()) ||
        overlaps(output.data(), output.size(), domain.data(), domain.size()) ||
        overlaps(output.data(), output.size(), nxm_key.data(), nxm_key.size()) ||
        overlaps(output.data(), output.size(), &written, sizeof(written)) ||
        overlaps(domain.data(), domain.size(), &written, sizeof(written)) ||
        overlaps(nxm_key.data(), nxm_key.size(), &written, sizeof(written))) return false;
    written = 0;
    clear_bytes(output.data(), output.size());
    if (output.empty() || !domain_valid(domain) || !mod_id || mod_id > max_identifier ||
        !file_id || file_id > max_identifier || nxm_key.size() > 1024 ||
        (nxm_key.empty() ? expires != 0 : !expires || expires > max_identifier)) return false;
    std::size_t encoded_size = 0;
    for (unsigned char c : nxm_key) {
        if (c < 33 || c > 126) return false;
        encoded_size += unreserved(c) ? 1 : 3;
    }
    constexpr std::string_view prefix = "https://api.nexusmods.com/v1/games/";
    constexpr std::string_view mods = "/mods/", files = "/files/", suffix = "/download_link";
    constexpr std::string_view key = "?key=", expiry = "&expires=";
    const auto size = prefix.size() + domain.size() + mods.size() + decimal_size(mod_id)
                    + files.size() + decimal_size(file_id) + suffix.size()
                    + (nxm_key.empty() ? 0 : key.size() + encoded_size + expiry.size() + decimal_size(expires));
    if (size >= output.size()) return false;
    char* destination = output.data();
    const auto append = [&destination](std::string_view value) noexcept {
        std::memcpy(destination, value.data(), value.size());
        destination += value.size();
    };
    append(prefix); append(domain); append(mods); write_decimal(destination, mod_id);
    append(files); write_decimal(destination, file_id); append(suffix);
    if (!nxm_key.empty()) {
        constexpr char hex[] = "0123456789ABCDEF";
        append(key);
        for (unsigned char c : nxm_key) {
            if (unreserved(c)) *destination++ = static_cast<char>(c);
            else {
                *destination++ = '%'; *destination++ = hex[c >> 4]; *destination++ = hex[c & 15];
            }
        }
        append(expiry); write_decimal(destination, expires);
    }
    *destination = 0;
    written = size;
    return true;
}

bool nexus_select_download_uri(std::span<const std::uint8_t> body,
    std::span<char> output, std::size_t& written) {
    if (output.size() > max_download_uri_bytes ||
        !valid_range(output.data(), output.size()) || !valid_range(body.data(), body.size()) ||
        overlaps(output.data(), output.size(), body.data(), body.size()) ||
        overlaps(output.data(), output.size(), &written, sizeof(written)) ||
        overlaps(body.data(), body.size(), &written, sizeof(written))) return false;
    written = 0;
    clear_bytes(output.data(), output.size());
    if (body.empty() || body.size() > max_body_bytes) return false;
    Json input;
    ParsedStringCleanup cleanup{input};
    try {
        ParseGate gate;
        input = Json::parse(body.begin(), body.end(),
            [&gate](int, Json::parse_event_t event, Json& value) {
                try { return gate.event(event, value); }
                catch (...) { scrub_json_strings(value); throw; }
            });
        const auto& mirrors = array(input);
        if (mirrors.empty() || mirrors.size() > 32) return false;
        for (const auto& mirror : mirrors) {
            if (!download_uri_valid(text(member(mirror, "URI"), max_download_uri_bytes - 1))) return false;
            (void)text(member(mirror, "name"), 256);
            (void)text(member(mirror, "short_name"), 64);
        }
        const auto& uri = member(mirrors.front(), "URI").get_ref<const std::string&>();
        // Only a fully validated response may report required storage. There
        // is no prefix or first-mirror publication on a malformed later item.
        written = uri.size();
        if (output.size() <= written) return false;
        std::memcpy(output.data(), uri.data(), written);
        output[written] = 0;
        return true;
    } catch (...) {
        written = 0;
        return false;
    }
}

bool nexus_oauth_code_form(std::string_view client, std::string_view redirect,
    std::string_view code, std::string_view verifier, std::span<char> output,
    std::size_t& written) noexcept {
    if (output.size() > 65536) return false;
    const std::array<std::pair<const void*, std::size_t>, 6> spans{{
        {client.data(),client.size()}, {redirect.data(),redirect.size()},
        {code.data(),code.size()}, {verifier.data(),verifier.size()},
        {output.data(),output.size()}, {&written,sizeof(written)}}};
    for (const auto& [data,size] : spans) if (!valid_range(data,size)) return false;
    for (std::size_t i=0; i<spans.size(); ++i)
        for (std::size_t j=i+1; j<spans.size(); ++j)
            if (overlaps(spans[i].first,spans[i].second,spans[j].first,spans[j].second)) return false;
    written=0; clear_bytes(output.data(),output.size());
    if (!oauth_fields_valid(client,redirect,code,verifier)) return false;
    constexpr std::array<std::string_view,4> labels{{
        "grant_type=authorization_code&redirect_uri=", "&client_id=", "&code=", "&code_verifier="}};
    const std::array<std::string_view,4> values{{redirect,client,code,verifier}};
    std::size_t needed=0;
    for (std::size_t i=0; i<values.size(); ++i) {
        needed+=labels[i].size();
        for (unsigned char c : values[i]) needed+=unreserved(c)?1:3;
    }
    if (needed>=output.size()) return false;
    char *destination=output.data();
    constexpr char hex[]="0123456789ABCDEF";
    for (std::size_t i=0; i<values.size(); ++i) {
        std::memcpy(destination,labels[i].data(),labels[i].size()); destination+=labels[i].size();
        for (unsigned char c : values[i]) {
            if (unreserved(c)) *destination++=static_cast<char>(c);
            else { *destination++='%'; *destination++=hex[c>>4]; *destination++=hex[c&15]; }
        }
    }
    *destination=0; written=needed;
    return true;
}

bool nexus_decode_oauth_tokens(std::span<const std::uint8_t> body,
    std::span<char> access, std::size_t& access_bytes,
    std::span<char> refresh, std::size_t& refresh_bytes,
    std::uint32_t& expires) {
    if (access.size()>16385 || refresh.size()>16385) return false;
    const std::array<std::pair<const void*,std::size_t>,6> spans{{
        {body.data(),body.size()}, {access.data(),access.size()}, {refresh.data(),refresh.size()},
        {&access_bytes,sizeof(access_bytes)}, {&refresh_bytes,sizeof(refresh_bytes)}, {&expires,sizeof(expires)}}};
    for (const auto& [data,size] : spans) if (!valid_range(data,size)) return false;
    for (std::size_t i=0; i<spans.size(); ++i)
        for (std::size_t j=i+1; j<spans.size(); ++j)
            if (overlaps(spans[i].first,spans[i].second,spans[j].first,spans[j].second)) return false;
    access_bytes=refresh_bytes=0; expires=0;
    clear_bytes(access.data(),access.size()); clear_bytes(refresh.data(),refresh.size());
    if (body.empty() || body.size()>131072 || access.empty() || refresh.empty()) return false;
    Json input;
    ParsedStringCleanup cleanup{input};
    try {
        ParseGate gate;
        input=Json::parse(body.begin(),body.end(),[&gate](int,Json::parse_event_t event,Json& value) {
            try { return gate.event(event,value); }
            catch (...) { scrub_json_strings(value); throw; }
        });
        const auto& a=text(member(input,"access_token"),16384);
        const auto& r=text(member(input,"refresh_token"),16384);
        const auto& type=text(member(input,"token_type"),6);
        const auto seconds=nonnegative_integer(member(input,"expires_in"));
        if (a.empty() || r.empty() || a.size()>16384 || r.size()>16384 ||
            type.size()!=6 || !seconds || seconds>31536000 ||
            access.size()<=a.size() || refresh.size()<=r.size()) return false;
        constexpr std::string_view bearer="bearer";
        for (std::size_t i=0; i<type.size(); ++i) {
            const unsigned char c=static_cast<unsigned char>(type[i]);
            if ((c>='A' && c<='Z' ? c+('a'-'A') : c)!=static_cast<unsigned char>(bearer[i])) return false;
        }
        for (const auto *token : {&a,&r})
            for (unsigned char c : *token) if (c<33 || c>126) return false;
        std::memcpy(access.data(),a.data(),a.size()); std::memcpy(refresh.data(),r.data(),r.size());
        access[a.size()]=refresh[r.size()]=0;
        access_bytes=a.size(); refresh_bytes=r.size(); expires=static_cast<std::uint32_t>(seconds);
        return true;
    } catch (const Failure&) { return false; }
      catch (const Json::exception&) { return false; }
}

Json decode_nexus_metadata(std::span<const std::uint8_t> body, NexusRecordKind kind,
                           std::string_view expected_domain, std::uint64_t expected_mod_id) {
    try {
        if (body.size() > max_body_bytes) throw Failure::ResourceLimit;
        ParseGate gate;
        const auto input = Json::parse(body.begin(), body.end(),
            [&gate](int, Json::parse_event_t event, Json& value) { return gate.event(event, value); });
        auto result = envelope("decoded");
        Json records = Json::array();
        switch (kind) {
        case NexusRecordKind::Games:
            for (const auto& item : array(input)) records.push_back(game_record(item));
            break;
        case NexusRecordKind::Mods:
            expected_identity(expected_domain, expected_mod_id, false);
            for (const auto& item : array(input)) records.push_back(mod_record(item, expected_domain, expected_mod_id));
            break;
        case NexusRecordKind::Mod:
            expected_identity(expected_domain, expected_mod_id, false);
            records.push_back(mod_record(input, expected_domain, expected_mod_id));
            break;
        case NexusRecordKind::Files:
            expected_identity(expected_domain, expected_mod_id, true);
            for (const auto& item : array(member(input, "files"))) records.push_back(file_record(item, expected_domain, expected_mod_id));
            break;
        case NexusRecordKind::OAuthUser:
            result["profile"] = oauth_profile(input);
            return result;
        case NexusRecordKind::ApiKeyUser:
            result["profile"] = api_key_profile(input);
            return result;
        default:
            throw Failure::Malformed;
        }
        // The locally built array is published only after every item validates.
        result["records"] = std::move(records);
        return result;
    } catch (Failure failure) {
        if (failure == Failure::ResourceLimit)
            return error_envelope("resource_limit", "nexus_metadata_resource_limit");
        return error_envelope("malformed", "nexus_metadata_malformed");
    } catch (const Json::exception&) {
        return error_envelope("malformed", "nexus_metadata_malformed");
    } catch (const std::bad_alloc&) {
        return error_envelope("internal_error", "nexus_metadata_internal_error");
    } catch (...) {
        return error_envelope("internal_error", "nexus_metadata_internal_error");
    }
}
} // namespace modbridge
