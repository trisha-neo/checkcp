#pragma once
#include <filesystem>
#include <string>

std::string sha256_file(const std::filesystem::path& path);

std::string copy_and_sha256(const std::filesystem::path& src, const std::filesystem::path& dst);

std::filesystem::path sidecar_path(const std::filesystem::path& file);

std::string read_sidecar(const std::filesystem::path& sidecar);

void write_sidecar(const std::filesystem::path& sidecar,
                   const std::string& hash,
                   const std::filesystem::path& file);
