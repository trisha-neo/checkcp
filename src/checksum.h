#pragma once
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Hash algorithm used for integrity sidecars.
enum class Algo { Sha256, Blake3 };

// Human-readable name: "sha256" / "blake3".
const char* algo_name(Algo algo);

// Sidecar file extension including the leading dot: ".sha256" / ".blake3".
std::string algo_extension(Algo algo);

// Parse "sha256"/"blake3" into an Algo; returns false on an unknown name.
bool parse_algo(const std::string& s, Algo& out);

// Compute the hash of a file; returns a 64-char lowercase hex string.
// (Both SHA-256 and BLAKE3 default output are 32 bytes -> 64 hex chars.)
std::string hash_file(const std::filesystem::path& path, Algo algo);

// Copy src to dst while computing the hash in one streaming pass; returns hex hash.
std::string copy_and_hash(const std::filesystem::path& src,
                          const std::filesystem::path& dst, Algo algo);

// Returns the path to the sidecar for a given file and algorithm.
std::filesystem::path sidecar_path(const std::filesystem::path& file, Algo algo);

// Read the stored hash from a sidecar file; returns empty string if missing or malformed.
std::string read_sidecar(const std::filesystem::path& sidecar);

// Write a sha256sum/b3sum-compatible sidecar: "<hash>  <filename>\n"
void write_sidecar(const std::filesystem::path& sidecar,
                   const std::string& hash,
                   const std::filesystem::path& file);

// Write a single {sha256,b3}sum-compatible sidecar for multiple files.
// Each entry is (path_relative_to_sidecar_dir, hash).
void write_dir_sidecar(const std::filesystem::path& sidecar,
                       const std::vector<std::pair<std::filesystem::path, std::string>>& entries);
