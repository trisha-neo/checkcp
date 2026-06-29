#pragma once
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Compute SHA-256 of a file; returns 64-char hex string
std::string sha256_file(const std::filesystem::path& path);

// Copy src to dst while computing SHA-256 in one streaming pass; returns hex hash of copied data
std::string copy_and_sha256(const std::filesystem::path& src, const std::filesystem::path& dst);

// Returns the path to the .sha256 sidecar for a given file
std::filesystem::path sidecar_path(const std::filesystem::path& file);

// Read the stored hash from a sidecar file; returns empty string if missing or malformed
std::string read_sidecar(const std::filesystem::path& sidecar);

// Write a sha256sum-compatible sidecar: "<hash>  <filename>\n"
void write_sidecar(const std::filesystem::path& sidecar,
                   const std::string& hash,
                   const std::filesystem::path& file);

// Write a single sha256sum-compatible sidecar for multiple files.
// Each entry is (path_relative_to_sidecar_dir, hash).
void write_dir_sidecar(const std::filesystem::path& sidecar,
                       const std::vector<std::pair<std::filesystem::path, std::string>>& entries);
