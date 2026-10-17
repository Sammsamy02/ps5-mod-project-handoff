// pool_admission.h — v1
//
// Tiny dependency-free INI reader for the [Seeds] PoolAdmission runtime
// switch (oracle|auto|shadow) used by the PS5 UE4SS composition.
//
// File format: standard UE4SS-style INI — [Section] headers, key=value
// lines, tolerated whitespace, comments starting with ';' or '#' (both
// full-line and trailing-after-value), CRLF tolerated, UTF-8 BOM skipped.
//
// Semantics:
//   * file missing/unreadable, [Seeds] missing, or key missing
//        -> { mode = Auto, present = false }
//   * unknown or empty value
//        -> { mode = Auto, present = true } + reason string
//   * recognized value (ASCII case-insensitive, surrounding ws trimmed)
//        -> { mode = its value, present = true }, reason = ""
//   * '=' is the only key/value delimiter (':' is NOT accepted)
//   * section, key and value matching are ASCII case-insensitive
//   * duplicate keys inside [Seeds]: the last occurrence wins
//   * file read is hard-capped at 64 KiB; bytes beyond the cap are ignored
//   * UTF-8 BOM is skipped; a UTF-16 BOM is rejected (reason set, Auto)
//   * pool_admission_reason() explains the latest fallback on this thread
//     and is valid until the next read_pool_admission() call
//
// C++17. Only <cstdio>/<cstddef>/<string>. No STL filesystem, no UE4SS
// types, no exceptions escape (read_pool_admission is noexcept). Reason
// storage is a plain static (single-threaded init discipline on PS5).

#ifndef L12_POOL_ADMISSION_H
#define L12_POOL_ADMISSION_H

#include <cstddef>
#include <cstdio>
#include <string>

enum class PoolAdmissionMode { Oracle, Auto, Shadow };

struct PoolAdmissionSetting {
    PoolAdmissionMode mode;  // effective mode; Auto on every fallback path
    bool present;            // true only when [Seeds] PoolAdmission was found
                             // (even if its value was unrecognized)
};

// Human-readable name ("oracle"|"auto"|"shadow") for logging.
inline const char* pool_admission_mode_name(PoolAdmissionMode mode) {
    switch (mode) {
        case PoolAdmissionMode::Oracle: return "oracle";
        case PoolAdmissionMode::Shadow: return "shadow";
        default:                         return "auto";
    }
}

namespace pool_admission_detail {

inline constexpr std::size_t kMaxIniBytes = 64 * 1024;

inline std::string& reason_storage() {
    static std::string storage;  // [L12-PS5] plain static: the seed path is single-threaded by the composition's init discipline; thread_local would pull __cxa_thread_atexit (undeclarable import on the pinned SDK)
    return storage;
}

inline bool is_trim_char(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

inline void trim_in_place(std::string& s) {
    std::size_t b = 0;
    std::size_t e = s.size();
    while (b < e && is_trim_char(s[b])) ++b;
    while (e > b && is_trim_char(s[e - 1])) --e;
    if (b != 0 || e != s.size()) s = s.substr(b, e - b);
}

inline char lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-insensitive compare of s against an ASCII lowercase literal.
inline bool equals_ci(const std::string& s, const char* lower_lit) {
    std::size_t i = 0;
    for (; i < s.size(); ++i) {
        if (lower_lit[i] == '\0') return false;  // s longer than literal
        if (lower_ascii(s[i]) != lower_lit[i]) return false;
    }
    return lower_lit[i] == '\0';                 // equal only if literal ended
}

}  // namespace pool_admission_detail

// Reason for the latest read_pool_admission() fallback on this thread.
// "" when the value parsed cleanly. Valid until the next call.
inline const char* pool_admission_reason() {
    return pool_admission_detail::reason_storage().c_str();
}

// Reads [Seeds] PoolAdmission from a UE4SS-style INI. Never throws.
inline PoolAdmissionSetting read_pool_admission(const char* ini_path) noexcept {
    using namespace pool_admission_detail;
    PoolAdmissionSetting result{PoolAdmissionMode::Auto, false};
    std::string& reason = reason_storage();
    reason.clear();
    try {
        if (ini_path == nullptr || ini_path[0] == '\0') {
            reason = "no ini path given";
            return result;  // default Auto
        }
        std::FILE* f = std::fopen(ini_path, "rb");
        if (f == nullptr) {
            reason = "ini file could not be opened";
            return result;  // default Auto
        }
        std::string buf;
        buf.resize(kMaxIniBytes);  // hard cap: never read more than 64 KiB
        const std::size_t n = std::fread(buf.data(), 1, kMaxIniBytes, f);
        std::fclose(f);
        buf.resize(n);

        // BOM handling: skip UTF-8 BOM, reject UTF-16 BOMs.
        const unsigned char b0 = n > 0 ? static_cast<unsigned char>(buf[0]) : 0;
        const unsigned char b1 = n > 1 ? static_cast<unsigned char>(buf[1]) : 0;
        const unsigned char b2 = n > 2 ? static_cast<unsigned char>(buf[2]) : 0;
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
            buf.erase(0, 3);
        } else if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF)) {
            reason = "utf-16 bom not supported (expected utf-8/no bom)";
            return result;  // default Auto
        }
        if (buf.empty()) {
            reason = "ini file is empty";
            return result;  // default Auto
        }

        bool in_seeds  = false;
        bool saw_seeds = false;
        bool found     = false;
        std::string last_value;

        std::size_t pos = 0;
        while (pos <= buf.size()) {
            const std::size_t nl = buf.find('\n', pos);
            std::string line = buf.substr(
                pos, (nl == std::string::npos ? buf.size() : nl) - pos);
            if (nl == std::string::npos && line.empty()) break;
            pos = (nl == std::string::npos) ? buf.size() + 1 : nl + 1;

            trim_in_place(line);
            if (line.empty()) continue;
            if (line[0] == ';' || line[0] == '#') continue;  // full-line comment
            const std::size_t c = line.find_first_of(";#");
            if (c != std::string::npos) line.erase(c);       // trailing comment
            trim_in_place(line);
            if (line.empty()) continue;

            if (line[0] == '[') {  // section header
                const std::size_t close = line.find(']');
                if (close == std::string::npos) {  // malformed header ends section
                    in_seeds = false;
                    continue;
                }
                in_seeds = equals_ci(line.substr(1, close - 1), "seeds");
                if (in_seeds) saw_seeds = true;
                continue;
            }

            const std::size_t eq = line.find('=');  // '=' is the only delimiter
            if (eq == std::string::npos) continue;  // ':' lines are not directives
            if (!in_seeds) continue;

            std::string key = line.substr(0, eq);
            trim_in_place(key);
            if (!equals_ci(key, "pooladmission")) continue;

            last_value = line.substr(eq + 1);
            trim_in_place(last_value);
            found = true;  // last occurrence wins
        }

        if (!saw_seeds) {
            reason = "[Seeds] section not found";
            return result;  // default Auto
        }
        if (!found) {
            reason = "PoolAdmission key not found in [Seeds]";
            return result;  // default Auto
        }

        result.present = true;
        if (last_value.empty()) {
            reason = "empty value for [Seeds] PoolAdmission (expected oracle|auto|shadow)";
            return result;  // mode stays Auto
        }
        if (equals_ci(last_value, "oracle")) {
            result.mode = PoolAdmissionMode::Oracle;
        } else if (equals_ci(last_value, "auto")) {
            result.mode = PoolAdmissionMode::Auto;
        } else if (equals_ci(last_value, "shadow")) {
            result.mode = PoolAdmissionMode::Shadow;
        } else {
            // Short SSO literals only; assignment will not allocate/throw.
            reason = "unknown value '" + last_value.substr(0, 32) +
                     "' for [Seeds] PoolAdmission (expected oracle|auto|shadow)";
            // mode stays Auto
        }
        return result;
    } catch (...) {
        // Defensively impossible today (no throwing ops except allocation).
        reason = "unexpected internal parse failure";
        return PoolAdmissionSetting{PoolAdmissionMode::Auto, false};
    }
}

#endif  // L12_POOL_ADMISSION_H
