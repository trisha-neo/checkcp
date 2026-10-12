#include "checksum.h"

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>

extern "C" {
#include "blake3/blake3.h"
}

static constexpr size_t BUF_SIZE = 65536;

namespace {

// Unified incremental hasher over the two backends (OpenSSL SHA-256 / BLAKE3).
class Hasher {
public:
    explicit Hasher(Algo algo) : algo_(algo) {
        if (algo_ == Algo::Blake3) {
            blake3_hasher_init(&b3_);
        } else {
            evp_ = EVP_MD_CTX_new();
            if (!evp_)
                throw std::runtime_error("EVP_MD_CTX_new failed");
            EVP_DigestInit_ex(evp_, EVP_sha256(), nullptr);
        }
    }
    ~Hasher() {
        if (evp_)
            EVP_MD_CTX_free(evp_);
    }
    Hasher(const Hasher&) = delete;
    Hasher& operator=(const Hasher&) = delete;

    void update(const void* data, size_t n) {
        if (algo_ == Algo::Blake3)
            blake3_hasher_update(&b3_, data, n);
        else
            EVP_DigestUpdate(evp_, data, n);
    }

    std::string hex() {
        unsigned char out[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        if (algo_ == Algo::Blake3) {
            len = BLAKE3_OUT_LEN;
            blake3_hasher_finalize(&b3_, out, len);
        } else {
            EVP_DigestFinal_ex(evp_, out, &len);
        }
        char hex[2 * EVP_MAX_MD_SIZE + 1];
        for (unsigned int i = 0; i < len; ++i)
            snprintf(hex + i * 2, 3, "%02x", out[i]);
        hex[len * 2] = '\0';
        return std::string(hex);
    }

private:
    Algo algo_;
    EVP_MD_CTX* evp_ = nullptr;
    blake3_hasher b3_{};
};

} // namespace

const char* algo_name(Algo algo) {
    return algo == Algo::Blake3 ? "blake3" : "sha256";
}

std::string algo_extension(Algo algo) {
    return algo == Algo::Blake3 ? ".blake3" : ".sha256";
}

bool parse_algo(const std::string& s, Algo& out) {
    if (s == "sha256") { out = Algo::Sha256; return true; }
    if (s == "blake3") { out = Algo::Blake3; return true; }
    return false;
}

std::string hash_file(const std::filesystem::path& path, Algo algo) {
    Hasher h(algo);

    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open: " + path.string());

    std::vector<char> buf(BUF_SIZE);
    while (in.read(buf.data(), static_cast<std::streamsize>(BUF_SIZE)) || in.gcount() > 0)
        h.update(buf.data(), static_cast<size_t>(in.gcount()));

    return h.hex();
}

std::string copy_and_hash(const std::filesystem::path& src,
                          const std::filesystem::path& dst, Algo algo) {
    Hasher h(algo);

    std::ifstream in(src, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open source: " + src.string());

    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Cannot open destination: " + dst.string());

    std::vector<char> buf(BUF_SIZE);
    while (in.read(buf.data(), static_cast<std::streamsize>(BUF_SIZE)) || in.gcount() > 0) {
        const size_t n = static_cast<size_t>(in.gcount());
        h.update(buf.data(), n);
        out.write(buf.data(), static_cast<std::streamsize>(n));
        if (!out)
            throw std::runtime_error("Write error to: " + dst.string());
    }

    return h.hex();
}

std::filesystem::path sidecar_path(const std::filesystem::path& file, Algo algo) {
    return std::filesystem::path(file.string() + algo_extension(algo));
}

std::string read_sidecar(const std::filesystem::path& sidecar) {
    std::ifstream f(sidecar);
    if (!f)
        return "";
    std::string hash;
    f >> hash;
    // Both SHA-256 and BLAKE3 default hex output are exactly 64 characters.
    return (hash.size() == 64) ? hash : "";
}

void write_sidecar(const std::filesystem::path& sidecar,
                   const std::string& hash,
                   const std::filesystem::path& file) {
    std::ofstream f(sidecar);
    if (!f)
        throw std::runtime_error("Cannot write sidecar: " + sidecar.string());
    // {sha256,b3}sum-compatible format: two spaces between hash and filename
    f << hash << "  " << file.filename().string() << "\n";
}

void write_dir_sidecar(const std::filesystem::path& sidecar,
                       const std::vector<std::pair<std::filesystem::path, std::string>>& entries) {
    std::ofstream f(sidecar);
    if (!f)
        throw std::runtime_error("Cannot write sidecar: " + sidecar.string());
    for (const auto& [rel, hash] : entries)
        f << hash << "  " << rel.string() << "\n";
}
