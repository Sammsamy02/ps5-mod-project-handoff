#include "modbridge/core.hpp"

#include <charconv>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;
using modbridge::Json;

namespace {
struct CliError : std::runtime_error {
    std::string code;
    CliError(std::string c, const std::string& message)
        : std::runtime_error(message), code(std::move(c)) {}
};

Json read_json(const fs::path& path) {
    constexpr std::size_t max_bytes = 67108864, max_nodes = 100000;
    constexpr int max_depth = 128;
    if (!fs::is_regular_file(path) || fs::file_size(path) > max_bytes)
        throw CliError("invalid_input", "Expected a JSON file of at most 64 MiB: " + path.string());
    std::ifstream file(path, std::ios::binary);
    if (!file) throw CliError("read_failed", "Could not open " + path.string());
    std::string content;
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        const auto size = static_cast<std::size_t>(file.gcount());
        if (size > max_bytes - content.size())
            throw CliError("invalid_input", "JSON input exceeded the byte limit while reading");
        content.append(buffer, size);
    }
    if (!file.eof()) throw CliError("read_failed", "Could not read " + path.string());
    std::size_t nodes = 0;
    const auto bounded = [&](int depth, Json::parse_event_t event, Json&) {
        if (depth > max_depth)
            throw CliError("invalid_input", "JSON input exceeded the nesting limit");
        if (event != Json::parse_event_t::object_end && event != Json::parse_event_t::array_end && ++nodes > max_nodes)
            throw CliError("invalid_input", "JSON input exceeded the node limit");
        return true;
    };
    return Json::parse(content, bounded);
}

std::uint64_t positive_integer(const std::string& text) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0)
        throw CliError("invalid_argument", "Expected a positive integer: " + text);
    return value;
}

std::map<std::string, std::string> options(int argc, char** argv, int begin,
                                         const std::set<std::string>& allowed) {
    std::map<std::string, std::string> result;
    for (int i = begin; i < argc; i += 2) {
        const std::string key = argv[i];
        if (!allowed.contains(key) || i + 1 >= argc || result.contains(key))
            throw CliError("invalid_argument", "Unknown, repeated or incomplete option: " + key);
        result.emplace(key, argv[i + 1]);
    }
    return result;
}

void emit(const Json& value, const std::map<std::string, std::string>& opts,
          std::initializer_list<fs::path> sources = {}) {
    const std::string serialized = value.dump(2) + "\n";
    if (!opts.contains("--output")) { std::cout << serialized; return; }
    const fs::path destination = opts.at("--output");
    if (fs::exists(destination) && !fs::is_regular_file(fs::symlink_status(destination)))
        throw CliError("write_failed", "Output must be a regular file path");
    for (const auto& source : sources)
        if (!source.empty() && fs::exists(destination) && fs::equivalent(source, destination))
            throw CliError("write_failed", "Output must not replace an input file");
        else if (!source.empty() && fs::is_directory(source)) {
            const auto directory = fs::weakly_canonical(source);
            const auto output = fs::weakly_canonical(destination);
            auto a = directory.begin(), b = output.begin();
            while (a != directory.end() && b != output.end() && *a == *b) { ++a; ++b; }
            if (a == directory.end())
                throw CliError("write_failed", "Output must be outside the input package directory");
        }
    fs::path temporary = destination;
    temporary += ".tmp." + std::to_string(::getpid());
    const int fd = ::open(temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    if (fd < 0) throw CliError("write_failed", "Could not create temporary report file");
    try {
        std::size_t offset = 0;
        while (offset < serialized.size()) {
            const auto count = ::write(fd, serialized.data() + offset, serialized.size() - offset);
            if (count <= 0) throw CliError("write_failed", "Could not write complete report");
            offset += static_cast<std::size_t>(count);
        }
        if (::fsync(fd) != 0) throw CliError("write_failed", "Could not synchronize report");
        if (::close(fd) != 0) throw CliError("write_failed", "Could not close report");
        if (::rename(temporary.c_str(), destination.c_str()) != 0)
            throw CliError("write_failed", "Could not publish report");
    } catch (...) {
        ::close(fd);
        std::error_code error;
        fs::remove(temporary, error);
        throw;
    }
}

void require_arguments(int argc, int minimum) {
    if (argc < minimum) throw CliError("invalid_argument", "Missing command arguments; use --help");
}

void scratch_init(const fs::path& root) {
    if (fs::is_symlink(fs::symlink_status(root)))
        throw CliError("invalid_root", "Scratch root cannot be a symlink");
    if (fs::exists(root) && (!fs::is_directory(root) || !fs::is_empty(root)))
        throw CliError("invalid_root", "Initialize a new or empty project-owned scratch directory");
    fs::create_directories(root);
    const auto marker = root / ".modbridge-scratch";
    std::ofstream file(marker, std::ios::binary);
    file << "modbridge scratch v1\n";
    file.close();
    if (!file) throw CliError("write_failed", "Could not create scratch marker");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require_arguments(argc, 2);
        const std::string command = argv[1];
        if (command == "--help" || command == "help") {
            if (argc != 2) throw CliError("invalid_argument", "Help accepts no extra arguments");
            std::cout << "ModBridge desktop development core\n"
                "analyze INPUT [--output REPORT] [--max-entries N] [--max-bytes N]\n"
                "              [--max-entry-bytes N] [--inspection-bytes N]\n"
                "plan INVENTORY --target TARGET --backends BACKENDS [--output REPORT]\n"
                "cache-key CONTEXT\nscratch-init ROOT\nscratch-deploy ROOT PREPARED MANIFEST [--fail-after N]\n"
                "scratch-recover ROOT\nscratch-remove ROOT TRANSACTION\n"
                "Console translation, Nexus authentication and game runtimes are not implemented yet.\n";
        } else if (command == "analyze") {
            require_arguments(argc, 3);
            const auto opts = options(argc, argv, 3, {"--output", "--max-entries", "--max-bytes", "--max-entry-bytes", "--inspection-bytes"});
            modbridge::Limits limits;
            if (opts.contains("--max-entries")) limits.max_entries = positive_integer(opts.at("--max-entries"));
            if (opts.contains("--max-bytes")) limits.max_total_bytes = positive_integer(opts.at("--max-bytes"));
            if (opts.contains("--max-entry-bytes")) limits.max_entry_bytes = positive_integer(opts.at("--max-entry-bytes"));
            if (opts.contains("--inspection-bytes")) limits.max_inspection_bytes = positive_integer(opts.at("--inspection-bytes"));
            const auto type = fs::symlink_status(argv[2]);
            if (!fs::is_regular_file(type) && !fs::is_directory(type))
                throw CliError("invalid_input", "Expected an existing package file or directory");
            emit(modbridge::analyze_package(argv[2], limits), opts, {argv[2]});
        } else if (command == "plan") {
            require_arguments(argc, 3);
            const auto opts = options(argc, argv, 3, {"--target", "--backends", "--output"});
            if (!opts.contains("--target") || !opts.contains("--backends"))
                throw CliError("invalid_argument", "Plan requires --target and --backends");
            emit(modbridge::build_plan(read_json(argv[2]), read_json(opts.at("--target")),
                                      read_json(opts.at("--backends"))), opts,
                                      {argv[2], opts.at("--target"), opts.at("--backends")});
        } else if (command == "cache-key") {
            if (argc != 3) throw CliError("invalid_argument", "cache-key requires one context file");
            std::cout << modbridge::cache_key(read_json(argv[2])) << '\n';
        } else if (command == "scratch-init") {
            if (argc != 3) throw CliError("invalid_argument", "scratch-init requires one root");
            scratch_init(argv[2]);
            emit({{"status", "initialized"}, {"scope", "desktop_scratch"}}, {});
        } else if (command == "scratch-deploy") {
            require_arguments(argc, 5);
            const auto opts = options(argc, argv, 5, {"--fail-after"});
            int fail_after = -1;
            if (opts.contains("--fail-after")) {
                const auto number = positive_integer(opts.at("--fail-after"));
                if (number > 2147483647) throw CliError("invalid_argument", "Failpoint is too large");
                fail_after = static_cast<int>(number);
            }
            emit(modbridge::deploy_scratch(argv[2], argv[3], read_json(argv[4]), fail_after), {});
        } else if (command == "scratch-recover") {
            if (argc != 3) throw CliError("invalid_argument", "scratch-recover requires one root");
            emit(modbridge::recover_scratch(argv[2]), {});
        } else if (command == "scratch-remove") {
            if (argc != 4) throw CliError("invalid_argument", "scratch-remove requires root and transaction ID");
            emit(modbridge::uninstall_scratch(argv[2], argv[3]), {});
        } else {
            throw CliError("invalid_argument", "Unknown command: " + command);
        }
        std::cout.flush();
        if (!std::cout) throw CliError("write_failed", "Could not write complete command output");
        return 0;
    } catch (const CliError& error) {
        std::cerr << Json{{"error", {{"code", error.code}, {"message", error.what()}}}}
                         .dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
    } catch (const std::exception& error) {
        std::cerr << Json{{"error", {{"code", "operation_failed"}, {"message", error.what()}}}}
                         .dump(-1, ' ', false, Json::error_handler_t::replace) << '\n';
    }
    return 2;
}
