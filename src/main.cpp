#include "checksum.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

struct Options {
    bool verbose    = false;
    bool preserve   = false;
    bool no_clobber = false;
    bool recursive  = false;
};

static void print_usage() {
    std::cerr <<
        "Usage: checkcp [OPTIONS] SOURCE DEST\n"
        "       checkcp [OPTIONS] SOURCE... DIRECTORY\n"
        "\n"
        "A drop-in replacement for cp that maintains SHA-256 integrity sidecars.\n"
        "Single file: a '<file>.sha256' sidecar is written alongside the destination.\n"
        "Multiple files or directory: a single 'checksums.sha256' is written in the\n"
        "destination directory, compatible with 'sha256sum -c checksums.sha256'.\n"
        "Warnings are emitted when source or destination integrity checks fail.\n"
        "\n"
        "Options:\n"
        "  -r, -R, --recursive  Copy directories recursively\n"
        "  -v, --verbose        Report each file copied\n"
        "  -p, --preserve       Preserve source file permissions\n"
        "  -n, --no-clobber     Do not overwrite existing destination files\n"
        "      --verify FILE    Verify FILE against its .sha256 sidecar and exit\n"
        "  -h, --help           Show this help\n";
}

static int verify_file(const fs::path& file) {
    if (!fs::exists(file)) {
        std::cerr << "checkcp: " << file << ": No such file or directory\n";
        return 1;
    const fs::path sc = sidecar_path(file);
    if (!fs::exists(sc)) {
        std::cerr << "checkcp: No sidecar found: " << sc << "\n";
        return 1;
    }
    const std::string stored = read_sidecar(sc);
    if (stored.empty()) {
        std::cerr << "checkcp: Sidecar is missing or malformed: " << sc << "\n";
        return 1;
    }
    std::string actual;
    try {
        actual = sha256_file(file);
    } catch (const std::exception& e) {
        std::cerr << "checkcp: " << e.what() << "\n";
        return 1;
    }
    if (actual == stored) {
        std::cout << file.filename().string() << ": OK\n";
        return 0;
    }
    std::cerr << file.filename().string() << ": FAILED\n"
              << "  Stored:   " << stored << "\n"
              << "  Computed: " << actual << "\n";
    return 1;
}

// Core copy: verify source, honour no-clobber, copy + hash. No sidecar is written.
// Returns true on success or skip; false on hard error.
// out_hash is set to the hash of the copied data, or empty if the file was skipped.
static bool copy_file_core(const fs::path& src, const fs::path& dst,
                            const Options& opts, int& warnings, int& copied,
                            std::string& out_hash) {
    out_hash.clear();

    // --- 1. Verify source integrity if a sidecar exists ---
    const fs::path src_sc = sidecar_path(src);
    if (fs::exists(src_sc)) {
        const std::string stored = read_sidecar(src_sc);
        if (!stored.empty()) {
            if (opts.verbose)
                std::cerr << "checkcp: Verifying source: " << src << "\n";
            try {
                const std::string actual = sha256_file(src);
                if (actual != stored) {
                    std::cerr << "checkcp: WARNING: Source integrity FAILED: " << src << "\n"
                              << "checkcp:   Stored:   " << stored << "\n"
                              << "checkcp:   Computed: " << actual << "\n"
                              << "checkcp:   Source may have been altered. Proceeding with copy.\n";
                    ++warnings;
                } else if (opts.verbose) {
                    std::cerr << "checkcp: Source OK: " << src << "\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "checkcp: WARNING: Cannot verify source: " << e.what() << "\n";
                ++warnings;
            }
        }
    }

    // --- 2. Honour no-clobber ---
    if (opts.no_clobber && fs::exists(dst)) {
        if (opts.verbose)
            std::cerr << "checkcp: Skipping (no-clobber): " << dst << "\n";
        return true;
    }

    // --- 3. Record existing destination hash before overwrite ---
    const fs::path dst_sc = sidecar_path(dst);
    const std::string prev_hash =
        (fs::exists(dst) && fs::exists(dst_sc)) ? read_sidecar(dst_sc) : "";

    if (opts.verbose)
        std::cerr << "checkcp: " << src.string() << " -> " << dst.string() << "\n";

    // --- 4. Copy + hash in one streaming pass ---
    try {
        out_hash = copy_and_sha256(src, dst);
    } catch (const std::exception& e) {
        std::cerr << "checkcp: ERROR: " << e.what() << "\n";
        return false;
    }

    // --- 5. Warn if destination content changed ---
    if (!prev_hash.empty() && prev_hash != out_hash) {
        std::cerr << "checkcp: WARNING: Destination replaced with different content: " << dst << "\n"
                  << "checkcp:   Previous: " << prev_hash << "\n"
                  << "checkcp:   New:      " << out_hash << "\n";
        ++warnings;
    }

    // --- 6. Optionally preserve permissions ---
    if (opts.preserve) {
        try {
            fs::permissions(dst, fs::status(src).permissions());
        } catch (...) {}
    }

    ++copied;
    return true;
}

// Single-file copy with a per-file '<dest>.sha256' sidecar.
static bool copy_with_integrity(const fs::path& src, const fs::path& dst,
                                 const Options& opts, int& warnings, int& copied) {
    std::string hash;
    if (!copy_file_core(src, dst, opts, warnings, copied, hash))
        return false;
    if (!hash.empty()) {
        try {
            write_sidecar(sidecar_path(dst), hash, dst);
        } catch (const std::exception& e) {
            std::cerr << "checkcp: WARNING: " << e.what() << "\n";
            ++warnings;
        }
    }
    return true;
}

// Recursively copy src_dir into dst_dir and write a single 'checksums.sha256'
// in dst_dir listing every copied file with its relative path.
// .sha256 files in the source tree are skipped — they are regenerated at the destination.
static void copy_dir_recursive(const fs::path& src_dir, const fs::path& dst_dir,
                                const Options& opts, int& warnings, int& errors, int& copied) {
    try {
        fs::create_directories(dst_dir);
        if (opts.preserve)
            fs::permissions(dst_dir, fs::status(src_dir).permissions());
    } catch (const std::exception& e) {
        std::cerr << "checkcp: ERROR: Cannot create directory " << dst_dir
                  << ": " << e.what() << "\n";
        ++errors;
        return;
    }

    std::vector<std::pair<fs::path, std::string>> entries;

    for (const auto& entry : fs::recursive_directory_iterator(
             src_dir, fs::directory_options::skip_permission_denied)) {

        const fs::path& src_entry = entry.path();

        // Skip .sha256 sidecars — they will be regenerated for the destination
        if (src_entry.extension() == ".sha256")
            continue;

        const fs::path rel       = fs::relative(src_entry, src_dir);
        const fs::path dst_entry = dst_dir / rel;

        if (entry.is_directory()) {
            try {
                fs::create_directories(dst_entry);
                if (opts.preserve)
                    fs::permissions(dst_entry, fs::status(src_entry).permissions());
            } catch (const std::exception& e) {
                std::cerr << "checkcp: ERROR: Cannot create directory " << dst_entry
                          << ": " << e.what() << "\n";
                ++errors;
            }
        } else if (entry.is_regular_file()) {
            std::string hash;
            if (copy_file_core(src_entry, dst_entry, opts, warnings, copied, hash)) {
                if (!hash.empty())
                    entries.emplace_back(rel, hash);
            } else {
                ++errors;
            }
        }
    }

    if (!entries.empty()) {
        try {
            write_dir_sidecar(dst_dir / "checksums.sha256", entries);
        } catch (const std::exception& e) {
            std::cerr << "checkcp: WARNING: " << e.what() << "\n";
            ++warnings;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    Options opts;
    std::vector<fs::path> positional;
    bool verify_mode = false;
    fs::path verify_target;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--verify") {
            if (i + 1 >= argc) {
                std::cerr << "checkcp: --verify requires a file argument\n";
                return 1;
            }
            verify_mode   = true;
            verify_target = argv[++i];
        } else if (arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--preserve") {
            opts.preserve = true;
        } else if (arg == "--no-clobber") {
            opts.no_clobber = true;
        } else if (arg == "--recursive") {
            opts.recursive = true;
        } else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
            // Combined short flags: -rv, -rp, etc.
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case 'v': opts.verbose    = true; break;
                    case 'p': opts.preserve   = true; break;
                    case 'n': opts.no_clobber = true; break;
                    case 'r': opts.recursive  = true; break;
                    case 'R': opts.recursive  = true; break;
                    default:
                        std::cerr << "checkcp: Unknown option: -" << arg[j] << "\n";
                        return 1;
                }
            }
        } else {
            positional.emplace_back(arg);
        }
    }

    if (verify_mode)
        return verify_file(verify_target);

    if (positional.size() < 2) {
        std::cerr << "checkcp: Missing source or destination operand\n";
        print_usage();
        return 1;
    }

    fs::path dest = positional.back();
    positional.pop_back();
    const std::vector<fs::path>& sources = positional;

    if (sources.size() > 1 && !fs::is_directory(dest)) {
        std::cerr << "checkcp: Target '" << dest
                  << "' is not a directory (required when copying multiple files)\n";
        return 1;
    }

    int warnings = 0;
    int errors   = 0;
    int copied   = 0;

    // When copying multiple source files into a directory, collect hashes for a
    // single checksums.sha256. Directory sources handle their own sidecar via
    // copy_dir_recursive. Single-file copies use a per-file sidecar.
    const bool use_shared_sidecar = (sources.size() > 1);
    std::vector<std::pair<fs::path, std::string>> shared_entries;

    for (const auto& src : sources) {
        if (!fs::exists(src)) {
            std::cerr << "checkcp: " << src << ": No such file or directory\n";
            ++errors;
            continue;
        }

        if (fs::is_directory(src)) {
            if (!opts.recursive) {
                std::cerr << "checkcp: " << src << ": Is a directory (use -r to copy recursively)\n";
                ++errors;
                continue;
            }
            // Mirror cp -r behaviour:
            //   dest exists as directory → copy src *into* dest (dest/src_name/)
            //   dest does not exist      → copy src *as* dest
            const fs::path actual_dst =
                fs::is_directory(dest) ? dest / src.filename() : dest;
            copy_dir_recursive(src, actual_dst, opts, warnings, errors, copied);
            continue;
        }

        const fs::path actual_dst =
            fs::is_directory(dest) ? dest / src.filename() : dest;

        if (use_shared_sidecar) {
            std::string hash;
            if (copy_file_core(src, actual_dst, opts, warnings, copied, hash)) {
                if (!hash.empty())
                    shared_entries.emplace_back(actual_dst.filename(), hash);
            } else {
                ++errors;
            }
        } else {
            if (!copy_with_integrity(src, actual_dst, opts, warnings, copied))
                ++errors;
        }
    }

    if (!shared_entries.empty()) {
        try {
            write_dir_sidecar(dest / "checksums.sha256", shared_entries);
        } catch (const std::exception& e) {
            std::cerr << "checkcp: WARNING: " << e.what() << "\n";
            ++warnings;
        }
    }

    // Always print a clear integrity status so the outcome is unambiguous
    if (errors == 0 && warnings == 0) {
        std::cerr << "checkcp: VERIFIED — " << copied << " file(s) copied and integrity confirmed.\n";
    } else if (errors == 0) {
        std::cerr << "checkcp: WARNING — " << copied << " file(s) copied but "
                  << warnings << " integrity issue(s) detected. Review warnings above.\n";
    } else {
        std::cerr << "checkcp: FAILED — " << errors << " error(s), "
                  << warnings << " warning(s). " << copied << " file(s) copied before failure.\n";
    }

    return (errors > 0) ? 1 : 0;
}