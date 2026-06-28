# checkcp

A drop-in replacement for `cp` that maintains SHA-256 integrity sidecars alongside every copied file. It verifies source integrity before copying and warns if a destination file is silently replaced with different content.

```bash
checkcp source/file.txt target/file.txt
# Produces: target/file.txt  +  target/file.txt.sha256
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
      --verify FILE    Verify FILE against its .sha256 sidecar and exit
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
```

## How it works

For every copy operation `checkcp`:

1. Checks whether the source has an existing `.sha256` sidecar and warns if the hash does not match.
2. Copies the file and computes its SHA-256 hash in a single streaming pass.
3. Warns if the destination previously existed with different content.
4. Writes a `.sha256` sidecar next to the destination file in sha256sum-compatible format.

Sidecar files follow the standard `sha256sum` format and can be verified independently:

```bash
sha256sum --check target.txt.sha256
```