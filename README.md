# checkcp

A drop-in replacement for `cp` that maintains SHA-256 integrity sidecars alongside every copied file. It verifies source integrity before copying and warns if a destination file is silently replaced with different content.

```bash
checkcp source/file.txt target/file.txt
# Produces: target/file.txt  +  target/file.txt.blake3
```

## Building from source

### Prerequisites

| Dependency | macOS | Linux |
|---|---|---|
| C++17 compiler | Xcode Command Line Tools (`xcode-select --install`) | `gcc` / `g++` (usually pre-installed) |
| OpenSSL | Homebrew (`brew install openssl`) | `libssl-dev` (Debian/Ubuntu) or `openssl-devel` (Fedora/RHEL) |
| pkg-config | `brew install pkg-config` | `pkg-config` package |

---

### Option A — GNU Make (quickest)

**macOS**

```bash
# 1. Install dependencies
xcode-select --install
brew install openssl pkg-config

# 2. Clone the repository
git clone https://github.com/<your-username>/checkcp.git
cd checkcp

# 3. Build
make

# 4. (Optional) Install to /usr/local/bin
sudo make install
```

**Linux (Debian / Ubuntu)**

```bash
# 1. Install dependencies
sudo apt update
sudo apt install -y g++ libssl-dev pkg-config

# 2. Clone the repository
git clone https://github.com/<your-username>/checkcp.git
cd checkcp

# 3. Build
make

# 4. (Optional) Install to /usr/local/bin
sudo make install
```

**Linux (Fedora / RHEL / Rocky)**

```bash
# 1. Install dependencies
sudo dnf install -y gcc-c++ openssl-devel pkg-config

# 2. Clone the repository
git clone https://github.com/<your-username>/checkcp.git
cd checkcp

# 3. Build
make

# 4. (Optional) Install to /usr/local/bin
sudo make install
```

The compiled binary is placed in the project root as `./checkcp`.

---

### Option B — CMake

**macOS**

```bash
# 1. Install dependencies
xcode-select --install
brew install openssl cmake

# 2. Clone the repository
git clone https://github.com/<your-username>/checkcp.git
cd checkcp

# 3. Configure
cmake -B build

# 4. Build
cmake --build build

# 5. (Optional) Install to /usr/local/bin
sudo cmake --install build
```

**Linux (Debian / Ubuntu)**

```bash
# 1. Install dependencies
sudo apt update
sudo apt install -y g++ libssl-dev cmake

# 2. Clone the repository
git clone https://github.com/<your-username>/checkcp.git
cd checkcp

# 3. Configure
cmake -B build

# 4. Build
cmake --build build

# 5. (Optional) Install to /usr/local/bin
sudo cmake --install build
```

**Linux (Fedora / RHEL / Rocky)**

```bash
# 1. Install dependencies
sudo dnf install -y gcc-c++ openssl-devel cmake

# 2. Clone the repository
git clone https://github.com/<your-username>/checkcp.git
cd checkcp

# 3. Configure
cmake -B build

# 4. Build
cmake --build build

# 5. (Optional) Install to /usr/local/bin
sudo cmake --install build
```

The compiled binary is placed at `build/checkcp`.

---

## Usage

```
Usage: checkcp [OPTIONS] SOURCE DEST
       checkcp [OPTIONS] SOURCE... DIRECTORY

Options:
  -r, -R, --recursive  Copy directories recursively
  -v, --verbose        Report each file copied
  -p, --preserve       Preserve source file permissions
  -n, --no-clobber     Do not overwrite existing destination files
      --algo NAME      Hash algorithm: sha256 (default) or blake3
      --verify FILE    Verify FILE against its sidecar and exit
  -h, --help           Show this help
```

**Examples**

```bash
# Copy a single file (creates target.txt and target.txt.sha256)
checkcp source.txt target.txt

# Copy multiple files into a directory
checkcp file1.txt file2.txt /dest/dir/

# Copy a directory recursively
checkcp -r source_dir/ /dest/dir/

# Verify an existing file against its sidecar
checkcp --verify target.txt

# Use BLAKE3 instead of SHA-256 (writes a .blake3 sidecar, verify with the same flag)
checkcp --algo blake3 source.txt target.txt
checkcp --algo blake3 --verify target.txt
```

## Hash algorithms

`checkcp` supports two algorithms, selected with `--algo`:

| Algorithm | Sidecar         | Default | Compatible checker            |
|-----------|-----------------|---------|-------------------------------|
| `blake3`  | `*.blake3`      | yes     | `b3sum -c`                    |
| `sha256`  | `*.sha256`      | no      | `sha256sum -c`, `shasum -a 256 -c` |

Both produce 64-character hex sidecars in the same `<hash>  <filename>` format,
so the same verification tooling applies (just swap `sha256sum` for `b3sum`).
The `--algo` flag applies to `--verify` as well and picks which sidecar to read.

**A note on speed:** BLAKE3 is dramatically faster than SHA-256 *on hardware
without SHA extensions* and *when multithreaded*. On CPUs with hardware SHA
acceleration (Apple Silicon, recent x86 with SHA-NI), OpenSSL's single-pass
SHA-256 is already ~1.5 GB/s, and `checkcp`'s single-threaded BLAKE3 will not
beat it. The BLAKE3 option is most useful on older/embedded hardware and as
groundwork for a future multithreaded hashing mode.

## How it works

For every copy operation `checkcp`:

1. Checks whether the source has an existing sidecar (for the selected algorithm) and warns if the hash does not match.
2. Copies the file and computes its hash in a single streaming pass.
3. Warns if the destination previously existed with different content.
4. Writes a sidecar in `sha256sum`/`b3sum`-compatible format.

**Single file** (`checkcp src dest`):
A `dest.sha256` sidecar is written alongside the destination file.

```bash
sha256sum --check dest.sha256
```

**Multiple files or directory** (`checkcp f1 f2 dest/` or `checkcp -r dir/ dest/`):
A single `checksums.sha256` is written inside the destination directory, with one line per file. Directory copies use relative paths so the whole tree can be verified in one command:

```bash
cd dest/
sha256sum --check checksums.sha256
```
