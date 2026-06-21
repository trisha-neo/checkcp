#include "checksum.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>

static constexpr size_t BUF_SIZE = 65536;

static std::string finalise_hex(EVP_MD_CTX* ctx) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);

    char hex[65];
    for (unsigned int i = 0; i < len; ++i)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    hex[len * 2] = '\0';
    return std::string(hex);
}

std::string sha256_file(const std::filesystem::path& path) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        throw std::runtime_error("EVP_MD_CTX_new failed");
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Cannot open: " + path.string());
    }

    std::vector<char> buf(BUF_SIZE);
    while (in.read(buf.data(), static_cast<std::streamsize>(BUF_SIZE)) || in.gcount() > 0)
        EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(in.gcount()));

    return finalise_hex(ctx);
}

std::string copy_and_sha256(const std::filesystem::path& src, const std::filesystem::path& dst) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        throw std::runtime_error("EVP_MD_CTX_new failed");
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    std::ifstream in(src, std::ios::binary);
    if (!in) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Cannot open source: " + src.string());
    }

    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Cannot open destination: " + dst.string());
    }

    std::vector<char> buf(BUF_SIZE);
    while (in.read(buf.data(), static_cast<std::streamsize>(BUF_SIZE)) || in.gcount() > 0) {
        const size_t n = static_cast<size_t>(in.gcount());
        EVP_DigestUpdate(ctx, buf.data(), n);
        out.write(buf.data(), static_cast<std::streamsize>(n));
        if (!out) {
            EVP_MD_CTX_free(ctx);
            throw std::runtime_error("Write error to: " + dst.string());
        }
    }

    return finalise_hex(ctx);
}

std::filesystem::path sidecar_path(const std::filesystem::path& file) {
    return std::filesystem::path(file.string() + ".sha256");
}

std::string read_sidecar(const std::filesystem::path& sidecar) {
    std::ifstream f(sidecar);
    if (!f)
        return "";
    std::string hash;
    f >> hash;
    return (hash.size() == 64) ? hash : "";
}

void write_sidecar(const std::filesystem::path& sidecar,
                   const std::string& hash,
                   const std::filesystem::path& file) {
    std::ofstream f(sidecar);
    if (!f)
        throw std::runtime_error("Cannot write sidecar: " + sidecar.string());
    f << hash << "  " << file.filename().string() << "\n";
}