#include "modbridge/nexus.hpp"

#include <cmath>
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
