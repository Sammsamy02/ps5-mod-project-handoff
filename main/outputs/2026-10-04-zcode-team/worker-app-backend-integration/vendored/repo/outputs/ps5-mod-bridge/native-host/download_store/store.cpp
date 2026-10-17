#include "store.h"
extern "C" {
#include "../archive/private/posix_compat.h"
}
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <picosha2/picosha2.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
using nlohmann::json;
struct mb_download_store {
  int fd;
  std::string root;
};
namespace {
constexpr uint64_t limit = 1073741824;
constexpr size_t response_limit = 262144;
bool private_file(const struct stat &s) {
  return S_ISREG(s.st_mode) && s.st_uid == geteuid() &&
         (s.st_mode & 0777) == 0600 && s.st_nlink == 1;
}
bool directory(int fd, struct stat &s) {
  return fstat(fd, &s) == 0 && S_ISDIR(s.st_mode) && s.st_uid == geteuid() &&
         (s.st_mode & 0777) == 0700;
}
bool hex(const std::string &s) {
  if (s.size() != 64)
    return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  return true;
}
bool game_ok(const std::string &s) {
  if (s.empty() || s.size() > 80)
    return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_'))
      return false;
  return true;
}
std::string digest(const std::string &s) {
  return picosha2::hash256_hex_string(s);
}
std::string id_for(const json &r) {
  return "dl-" + digest(r.at("game").get<std::string>() + "\n" +
                        std::to_string(r.at("mod_id").get<uint64_t>()) + "\n" +
                        std::to_string(r.at("file_id").get<uint64_t>()) + "\n" +
                        r.at("sha256").get<std::string>());
}
long mtime_ns(const struct stat &s) {
#if defined(__APPLE__)
  return s.st_mtimespec.tv_nsec;
#else
  return s.st_mtim.tv_nsec;
#endif
}
long ctime_ns(const struct stat &s) {
#if defined(__APPLE__)
  return s.st_ctimespec.tv_nsec;
#else
  return s.st_ctim.tv_nsec;
#endif
}
bool stable(const struct stat &a, const struct stat &b) {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino &&
         a.st_size == b.st_size && a.st_mtime == b.st_mtime &&
         a.st_ctime == b.st_ctime && mtime_ns(a) == mtime_ns(b) &&
         ctime_ns(a) == ctime_ns(b);
}
int scan(int fd, uint64_t bytes, const std::string &sha, int copy = -1) {
  struct stat a{}, b{};
  if (fstat(fd, &a) || !S_ISREG(a.st_mode) || a.st_size < 0 ||
      uint64_t(a.st_size) != bytes)
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  picosha2::hash256_one_by_one h;
  h.init();
  std::array<unsigned char, 65536> buf{};
  uint64_t at = 0;
  while (at < bytes) {
    size_t want = size_t(std::min<uint64_t>(buf.size(), bytes - at));
    ssize_t n = pread(fd, buf.data(), want, off_t(at));
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
    h.process(buf.begin(), buf.begin() + n);
    if (copy >= 0) {
      ssize_t p = 0;
      while (p < n) {
        ssize_t w = write(copy, buf.data() + p, size_t(n - p));
        if (w < 0 && errno == EINTR)
          continue;
        if (w <= 0)
          return MB_DOWNLOAD_STORE_STORAGE_FAILED;
        p += w;
      }
    }
    at += uint64_t(n);
  }
  h.finish();
  if (fstat(fd, &b) || !stable(a, b) || picosha2::get_hash_hex_string(h) != sha)
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  return 0;
}
int emit(const json &r, char *out, size_t cap) {
  std::string s = r.dump();
  if (s.size() + 1 > cap || s.size() + 1 > response_limit)
    return MB_DOWNLOAD_STORE_OUTPUT_TOO_SMALL;
  memcpy(out, s.c_str(), s.size() + 1);
  return 0;
}
int archive(mb_download_store *c, const json &r, bool full = true) {
  std::string name = r.at("download_id").get<std::string>() + ".archive";
  int fd = mb_archive_posix_openat(c->fd, name.c_str(),
                                   O_RDONLY | O_NOFOLLOW | O_CLOEXEC, 0);
  if (fd < 0)
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  struct stat s{};
  int rc = fstat(fd, &s) || !private_file(s)
               ? MB_DOWNLOAD_STORE_INTEGRITY_FAILED
               : (uint64_t(s.st_size) != r.at("bytes").get<uint64_t>()
                      ? MB_DOWNLOAD_STORE_INTEGRITY_FAILED
                      : (full ? scan(fd, r.at("bytes").get<uint64_t>(),
                                     r.at("sha256").get<std::string>())
                              : 0));
  if (!rc && r.contains("archive_ino") &&
      (r.at("archive_ino") != uint64_t(s.st_ino) ||
       r.at("archive_dev") != uint64_t(s.st_dev) ||
       r.at("archive_mtime") != int64_t(s.st_mtime) ||
       r.at("archive_ctime") != int64_t(s.st_ctime) ||
       r.at("archive_mtime_ns") != mtime_ns(s) ||
       r.at("archive_ctime_ns") != ctime_ns(s)))
    rc = MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  close(fd);
  return rc;
}
bool record(const json &r) {
  try {
    static const char *strings[] = {"download_id", "state", "game", "sha256"};
    static const char *unsigneds[] = {"mod_id",  "file_id",     "bytes",
                                      "version", "archive_dev", "archive_ino"};
    static const char *signeds[] = {"archive_mtime", "archive_ctime",
                                    "archive_mtime_ns", "archive_ctime_ns"};
    if (!r.is_object() || r.size() != 15)
      return false;
    for (auto key : strings)
      if (!r.at(key).is_string())
        return false;
    for (auto key : unsigneds)
      if (!r.at(key).is_number_unsigned())
        return false;
    for (auto key : signeds)
      if (!r.at(key).is_number_integer())
        return false;
    return r.at("retained").is_boolean() && r.at("retained") == true &&
           r.at("state") == "downloaded" && game_ok(r.at("game")) &&
           r.at("mod_id").get<uint64_t>() > 0 &&
           r.at("file_id").get<uint64_t>() > 0 &&
           r.at("bytes").get<uint64_t>() <= limit && hex(r.at("sha256")) &&
           r.at("download_id") == id_for(r) && r.at("version") == 1 &&
           r.at("archive_mtime_ns").get<int64_t>() >= 0 &&
           r.at("archive_mtime_ns").get<int64_t>() < 1000000000 &&
           r.at("archive_ctime_ns").get<int64_t>() >= 0 &&
           r.at("archive_ctime_ns").get<int64_t>() < 1000000000;
  } catch (...) {
    return false;
  }
}

int load(mb_download_store *c, const std::string &id, json &r,
         bool full = true) {
  if (id.size() != 67 || id.substr(0, 3) != "dl-" || !hex(id.substr(3)))
    return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
  std::string name = id + ".json";
  int fd = mb_archive_posix_openat(c->fd, name.c_str(),
                                   O_RDONLY | O_NOFOLLOW | O_CLOEXEC, 0);
  if (fd < 0)
    return errno == ENOENT ? MB_DOWNLOAD_STORE_NOT_FOUND
                           : MB_DOWNLOAD_STORE_STORAGE_FAILED;
  struct stat s{};
  if (fstat(fd, &s) ||
      !(S_ISREG(s.st_mode) && s.st_uid == geteuid() &&
        (s.st_mode & 0777) == 0600 && s.st_nlink >= 1 && s.st_nlink <= 2) ||
      s.st_size <= 0 || s.st_size > 4096) {
    close(fd);
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  }
  std::string text(size_t(s.st_size), '\0');
  size_t at = 0;
  while (at < text.size()) {
    ssize_t n = read(fd, &text[at], text.size() - at);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0) {
      close(fd);
      return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
    }
    at += size_t(n);
  }
  close(fd);
  r = json::parse(text, nullptr, false);
  if (!record(r) || r.at("download_id") != id)
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  return archive(c, r, full);
}

constexpr size_t metadata_wire_limit = MB_DOWNLOAD_STORE_METADATA_LIMIT * 6u + 1024u;
// Validate shape with bounded SAX storage before any DOM parse. Metadata
// semantics (source/requirements fields) belong to the registration adapter.
struct MetadataShape : nlohmann::json_sax<json> {
  struct Frame { bool object; std::set<std::string> keys; };
  std::vector<Frame> frames;
  size_t events = 0;
  bool started = false;
  bool event() { return ++events <= 200000; }
  bool scalar() { return started && !frames.empty() && event(); }
  bool null() override { return scalar(); }
  bool boolean(bool) override { return scalar(); }
  bool number_integer(number_integer_t) override { return scalar(); }
  bool number_unsigned(number_unsigned_t) override { return scalar(); }
  bool number_float(number_float_t, const string_t &) override { return scalar(); }
  bool string(string_t &) override { return scalar(); }
  bool binary(binary_t &) override { return false; }
  bool start(bool object) {
    if (!event() || frames.size() >= 64 || (!started && !object) ||
        (started && frames.empty())) return false;
    started = true;
    frames.push_back({object, {}});
    return true;
  }
  bool start_object(std::size_t) override { return start(true); }
  bool start_array(std::size_t) override { return start(false); }
  bool key(string_t &value) override {
    return event() && !frames.empty() && frames.back().object &&
           frames.back().keys.insert(value).second;
  }
  bool end_object() override { return end(true); }
  bool end_array() override { return end(false); }
  bool end(bool object) {
    if (!event() || frames.empty() || frames.back().object != object) return false;
    frames.pop_back();
    return true;
  }
  bool parse_error(std::size_t, const std::string &,
                   const nlohmann::detail::exception &) override { return false; }
};
bool metadata_object(const char *text, size_t bytes, size_t maximum) {
  if (!text || !bytes || bytes > maximum || std::memchr(text, 0, bytes)) return false;
  MetadataShape shape;
  return json::sax_parse(text, text + bytes, &shape) && shape.started && shape.frames.empty();
}
struct OwnedFd {
  int value;
  ~OwnedFd() { if (value >= 0) close(value); }
};
struct PendingMetadata {
  mb_download_store *store;
  std::string name;
  bool owned = false;
  ~PendingMetadata() {
    if (owned) mb_archive_posix_unlinkat(store->fd, name.c_str(), 0);
  }
};
bool metadata_file(const struct stat &s) {
  return S_ISREG(s.st_mode) && s.st_uid == geteuid() &&
         (s.st_mode & 0777) == 0600 && s.st_nlink >= 1 && s.st_nlink <= 2 &&
         s.st_size > 0 && uint64_t(s.st_size) <= metadata_wire_limit;
}
int read_metadata(mb_download_store *c, const std::string &id, const json &record,
                  std::string &metadata) {
  std::string name = id + ".metadata.json";
  OwnedFd input{mb_archive_posix_openat(c->fd, name.c_str(),
      O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC, 0)};
  if (input.value < 0)
    return errno == ENOENT ? MB_DOWNLOAD_STORE_NOT_FOUND : MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  struct stat before{}, after{}, named{};
  if (fstat(input.value, &before) || !metadata_file(before)) return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  std::string bytes(size_t(before.st_size), '\0');
  size_t at = 0;
  while (at < bytes.size()) {
    ssize_t n = read(input.value, &bytes[at], bytes.size() - at);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
    at += size_t(n);
  }
  if (fstat(input.value, &after) || !metadata_file(after) || !stable(before, after) ||
      mb_archive_posix_fstatat(c->fd, name.c_str(), &named, AT_SYMLINK_NOFOLLOW) ||
      !metadata_file(named) || !stable(after, named) ||
      !metadata_object(bytes.data(), bytes.size(), metadata_wire_limit))
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  const auto envelope = json::parse(bytes, nullptr, false);
  if (!envelope.is_object() || envelope.size() != 5 ||
      !envelope.contains("version") || !envelope["version"].is_number_unsigned() || envelope["version"] != 1 ||
      !envelope.contains("download_id") || envelope["download_id"] != id ||
      !envelope.contains("archive_record_sha256") || envelope["archive_record_sha256"] != digest(record.dump()) ||
      !envelope.contains("metadata_sha256") || !envelope["metadata_sha256"].is_string() ||
      !envelope.contains("metadata") || !envelope["metadata"].is_string())
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  auto captured = envelope["metadata"].get<std::string>();
  if (!metadata_object(captured.data(), captured.size(), MB_DOWNLOAD_STORE_METADATA_LIMIT) ||
      envelope["metadata_sha256"] != digest(captured) || archive(c, record, false))
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  metadata = std::move(captured);
  return MB_DOWNLOAD_STORE_OK;
}
json public_record(const json &r) {
  json out = json::object();
  for (auto key : {"download_id", "state", "game", "mod_id", "file_id",
                   "sha256", "bytes", "retained"})
    out[key] = r.at(key);
  return out;
}

int records(mb_download_store *c, json &out, size_t &count) {
  int duplicate = mb_archive_posix_openat(
      c->fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
  if (duplicate < 0)
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  DIR *d = fdopendir(duplicate);
  if (!d) {
    close(duplicate);
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  }
  count = 0;
  out = json::array();
  while (auto *e = readdir(d)) {
    std::string n = e->d_name;
    if (n.size() != 72 || n.substr(67) != ".json" || n.substr(0, 3) != "dl-")
      continue;
    if (++count > 256) {
      closedir(d);
      return MB_DOWNLOAD_STORE_LIMIT_REACHED;
    }
    json r;
    if (load(c, n.substr(0, 67), r, false) == 0)
      out.push_back(public_record(r));
  }
  closedir(d);
  return 0;
}
} // namespace
extern "C" int mb_download_store_open(int fd, const char *root,
                                      mb_download_store **out) {
  if (!out)
    return 1;
  *out = nullptr;
  struct stat s{};
  if (!directory(fd, s))
    return 1;
  try {
    std::string path;
    if (root) {
      if (root[0] != '/' || strlen(root) > 4096)
        return 1;
      char *canonical = realpath(root, nullptr);
      if (!canonical)
        return 1;
      path = canonical;
      free(canonical);
      if (path != root)
        return 1;
      int f = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      struct stat t{};
      bool ok = f >= 0 && directory(f, t) && t.st_dev == s.st_dev &&
                t.st_ino == s.st_ino;
      if (f >= 0)
        close(f);
      if (!ok)
        return 1;
    }
    *out = new mb_download_store{fd, path};
    return 0;
  } catch (...) {
    return 2;
  }
}
extern "C" void mb_download_store_free(mb_download_store *c) { delete c; }
extern "C" int mb_download_store_list(mb_download_store *c, char *out,
                                      size_t cap) {
  if (out && cap)
    out[0] = 0;
  if (!c || !out || !cap)
    return 1;
  try {
    json a;
    size_t count;
    int rc = records(c, a, count);
    return rc ? rc : emit(json{{"items", a}}, out, cap);
  } catch (...) {
    return 2;
  }
}
extern "C" int mb_download_store_lookup(mb_download_store *c, const char *id,
                                        char *out, size_t cap) {
  if (out && cap)
    out[0] = 0;
  if (!c || !id || !out || !cap)
    return 1;
  try {
    json r;
    int rc = load(c, id, r);
    if (rc)
      return rc;
    if (c->root.empty())
      return 7;
    struct stat a{}, b{};
    int f =
        open(c->root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    bool ok = f >= 0 && directory(c->fd, a) && directory(f, b) &&
              a.st_dev == b.st_dev && a.st_ino == b.st_ino;
    if (f >= 0)
      close(f);
    if (!ok)
      return 7;
    r = public_record(r);
    r["archive_path"] = c->root + "/" + id + ".archive";
    return emit(r, out, cap);
  } catch (...) {
    return 2;
  }
}
extern "C" int mb_download_store_lookup_metadata(mb_download_store *c, const char *id,
                                                  char *out, size_t cap) {
  if (out && cap) out[0] = 0;
  if (!c || !id || !out || !cap) return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
  try {
    json record;
    int rc = load(c, id, record, false);
    if (rc) return rc;
    std::string metadata;
    rc = read_metadata(c, id, record, metadata);
    if (rc) return rc;
    if (metadata.size() + 1 > cap) return MB_DOWNLOAD_STORE_OUTPUT_TOO_SMALL;
    std::memcpy(out, metadata.c_str(), metadata.size() + 1);
    return MB_DOWNLOAD_STORE_OK;
  } catch (...) { return MB_DOWNLOAD_STORE_STORAGE_FAILED; }
}
extern "C" int mb_download_store_publish_metadata(mb_download_store *c, const char *id,
                                                   const char *text, size_t bytes) {
  if (!c || !id || !text || !bytes || bytes > MB_DOWNLOAD_STORE_METADATA_LIMIT)
    return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
  try {
    if (!metadata_object(text, bytes, MB_DOWNLOAD_STORE_METADATA_LIMIT)) return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
    json record;
    int rc = load(c, id, record, false);
    if (rc) return rc;
    const std::string metadata(text, bytes);
    std::string previous;
    rc = read_metadata(c, id, record, previous);
    if (rc == MB_DOWNLOAD_STORE_OK) {
      if (previous != metadata) return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
      // A prior publication may have returned a directory-sync failure after
      // committing the final name. Replay finishes that durability obligation.
      return fsync(c->fd) ? MB_DOWNLOAD_STORE_STORAGE_FAILED : MB_DOWNLOAD_STORE_OK;
    }
    if (rc != MB_DOWNLOAD_STORE_NOT_FOUND) return rc;
    const std::string serialized = json{{"version", 1}, {"download_id", id},
        {"archive_record_sha256", digest(record.dump())}, {"metadata_sha256", digest(metadata)},
        {"metadata", metadata}}.dump();
    if (serialized.size() > metadata_wire_limit) return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
    static unsigned long sequence = 0;
    const std::string name = std::string(id) + ".metadata.json";
    PendingMetadata pending{c, std::string(id) + ".metadata-pending-" +
        std::to_string(getpid()) + "-" + std::to_string(++sequence)};
    OwnedFd dest{mb_archive_posix_openat(c->fd, pending.name.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)};
    if (dest.value < 0) return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    pending.owned = true;
    size_t at = 0;
    while (at < serialized.size()) {
      ssize_t n = write(dest.value, serialized.data() + at, serialized.size() - at);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) return MB_DOWNLOAD_STORE_STORAGE_FAILED;
      at += size_t(n);
    }
    struct stat written{};
    if (fstat(dest.value, &written) || !private_file(written) ||
        uint64_t(written.st_size) != serialized.size() || fsync(dest.value))
      return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    const int closed = close(dest.value);
    dest.value = -1;
    if (closed) return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    // The binding must remain current across staging, before its commit point.
    json current;
    rc = load(c, id, current, false);
    if (rc) return rc;
    if (current != record) return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
    if (mb_archive_posix_linkat(c->fd, pending.name.c_str(), c->fd, name.c_str(), 0)) {
      if (errno != EEXIST) return MB_DOWNLOAD_STORE_STORAGE_FAILED;
      rc = read_metadata(c, id, record, previous);
      if (rc) return rc;
      if (previous != metadata) return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
      return fsync(c->fd) ? MB_DOWNLOAD_STORE_STORAGE_FAILED : MB_DOWNLOAD_STORE_OK;
    }
    if (mb_archive_posix_unlinkat(c->fd, pending.name.c_str(), 0)) return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    pending.owned = false;
    if (fsync(c->fd)) return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    return MB_DOWNLOAD_STORE_OK;
  } catch (...) { return MB_DOWNLOAD_STORE_STORAGE_FAILED; }
}
namespace {
// Recover only a fully verified final archive with one matching generated
// staging link.
int recover_link(mb_download_store *c, const std::string &id, const json &r) {
  std::string final = id + ".archive";
  int fd = mb_archive_posix_openat(c->fd, final.c_str(),
                                   O_RDONLY | O_NOFOLLOW | O_CLOEXEC, 0);
  if (fd < 0)
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  struct stat st{};
  int rc = fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
                   (st.st_mode & 0777) != 0600 || st.st_nlink != 2
               ? MB_DOWNLOAD_STORE_INTEGRITY_FAILED
               : scan(fd, r.at("bytes"), r.at("sha256"));
  close(fd);
  if (rc)
    return rc;
  int duplicate = mb_archive_posix_openat(
      c->fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
  if (duplicate < 0)
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  DIR *d = fdopendir(duplicate);
  if (!d) {
    close(duplicate);
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  }
  std::string prefix = id + ".staging-", matching;
  while (auto *entry = readdir(d)) {
    std::string name = entry->d_name;
    if (name.compare(0, prefix.size(), prefix) != 0)
      continue;
    std::string suffix = name.substr(prefix.size());
    if (suffix.empty() ||
        suffix.find_first_not_of("0123456789-") != std::string::npos)
      continue;
    struct stat other{};
    if (!mb_archive_posix_fstatat(c->fd, name.c_str(), &other,
                                  AT_SYMLINK_NOFOLLOW) &&
        other.st_dev == st.st_dev && other.st_ino == st.st_ino &&
        other.st_nlink == 2) {
      matching = name;
      break;
    }
  }
  closedir(d);
  if (matching.empty())
    return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
  if (mb_archive_posix_unlinkat(c->fd, matching.c_str(), 0) || fsync(c->fd))
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  return archive(c, r);
}
} // namespace
extern "C" int mb_download_store_publish(mb_download_store *c, int src,
                                         const char *game, uint64_t mod,
                                         uint64_t file, uint64_t bytes,
                                         const char *sha, char *out,
                                         size_t cap) {
  if (out && cap)
    out[0] = 0;
  if (!c || !game || !sha || !out || !cap || !game_ok(game) || !hex(sha) ||
      !mod || !file || bytes > limit || src < 0)
    return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
  try {
    json r = {{"game", game},     {"mod_id", mod}, {"file_id", file},
              {"bytes", bytes},   {"sha256", sha}, {"state", "downloaded"},
              {"retained", true}, {"version", 1}};
    std::string id = id_for(r);
    r["download_id"] = id;
    if (public_record(r).dump().size() + 1 > cap)
      return MB_DOWNLOAD_STORE_OUTPUT_TOO_SMALL;
    int flags = fcntl(src, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY)
      return MB_DOWNLOAD_STORE_BAD_ARGUMENT;
    json existing;
    int rc = load(c, id, existing);
    if (rc == 0) {
      rc = scan(src, bytes, sha);
      if (rc)
        return rc;
      return public_record(existing) == public_record(r)
                 ? emit(public_record(existing), out, cap)
                 : MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
    }
    if (rc != MB_DOWNLOAD_STORE_NOT_FOUND)
      return rc;
    json all;
    size_t count;
    rc = records(c, all, count);
    if (rc)
      return rc;
    if (count >= 256)
      return MB_DOWNLOAD_STORE_LIMIT_REACHED;
    std::string name = id + ".archive";
    struct stat final_stat{};
    bool exists = mb_archive_posix_fstatat(c->fd, name.c_str(), &final_stat,
                                           AT_SYMLINK_NOFOLLOW) == 0;
    if (!exists && errno != ENOENT)
      return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    static unsigned long sequence = 0;
    std::string unique =
        std::to_string(getpid()) + "-" + std::to_string(++sequence);
    if (exists) {
      rc = scan(src, bytes, sha);
      if (rc)
        return rc;
      rc = archive(c, r);
      if (rc && final_stat.st_nlink == 2)
        rc = recover_link(c, id, r);
      if (rc)
        return rc;
    } else {
      // A crash during copy leaves only this uncommitted generated staging
      // file. Retry uses a fresh exclusive name and never replaces the final
      // archive.
      std::string staging = id + ".staging-" + unique;
      int dest = mb_archive_posix_openat(
          c->fd, staging.c_str(),
          O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
      if (dest < 0)
        return MB_DOWNLOAD_STORE_STORAGE_FAILED;
      rc = scan(src, bytes, sha, dest);
      struct stat copied{};
      if (!rc && (fstat(dest, &copied) || !private_file(copied) ||
                  uint64_t(copied.st_size) != bytes || fsync(dest)))
        rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
      close(dest);
      if (!rc && mb_archive_posix_linkat(c->fd, staging.c_str(), c->fd,
                                         name.c_str(), 0))
        rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
      if (mb_archive_posix_unlinkat(c->fd, staging.c_str(), 0) && !rc)
        rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
      if (rc)
        return rc;
      if (fsync(c->fd))
        return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    }
    struct stat stored{};
    if (mb_archive_posix_fstatat(c->fd, name.c_str(), &stored,
                                 AT_SYMLINK_NOFOLLOW) ||
        !private_file(stored) || uint64_t(stored.st_size) != bytes)
      return MB_DOWNLOAD_STORE_INTEGRITY_FAILED;
    r["archive_dev"] = uint64_t(stored.st_dev);
    r["archive_ino"] = uint64_t(stored.st_ino);
    r["archive_mtime"] = int64_t(stored.st_mtime);
    r["archive_ctime"] = int64_t(stored.st_ctime);
    r["archive_mtime_ns"] = mtime_ns(stored);
    r["archive_ctime_ns"] = ctime_ns(stored);
    std::string meta = id + ".json", pending = id + ".pending-" + unique;
    int m = mb_archive_posix_openat(
        c->fd, pending.c_str(),
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (m < 0)
      return MB_DOWNLOAD_STORE_STORAGE_FAILED;
    std::string text = r.dump();
    size_t at = 0;
    while (at < text.size()) {
      ssize_t n = write(m, text.data() + at, text.size() - at);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0) {
        rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
        break;
      }
      at += size_t(n);
    }
    if (!rc && fsync(m))
      rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
    close(m);
    if (!rc &&
        mb_archive_posix_linkat(c->fd, pending.c_str(), c->fd, meta.c_str(), 0))
      rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
    mb_archive_posix_unlinkat(c->fd, pending.c_str(), 0);
    if (!rc && fsync(c->fd))
      rc = MB_DOWNLOAD_STORE_STORAGE_FAILED;
    return rc ? rc : emit(public_record(r), out, cap);
  } catch (...) {
    return MB_DOWNLOAD_STORE_STORAGE_FAILED;
  }
}
