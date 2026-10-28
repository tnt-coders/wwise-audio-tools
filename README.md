# Wwise Audio Tools

[![Build - Windows](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/build-windows.yml/badge.svg)](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/build-windows.yml)
[![Build - Linux](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/build-linux.yml/badge.svg)](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/build-linux.yml)
[![Build - macOS](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/build-macos.yml/badge.svg)](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/build-macos.yml)
[![Docs](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/docs.yml/badge.svg)](https://github.com/tnt-coders/wwise-audio-tools/actions/workflows/docs.yml)

A C++ library and command-line tool for converting Wwise WEM audio files to OGG Vorbis format and extracting WEM files from BNK soundbanks.

## About

Wwise encodes audio as `.wem` files using a modified Vorbis format with stripped headers, custom packet framing, and external codebooks. The traditional approach to decoding these requires chaining two separate tools (`ww2ogg` + `revorb`) with manual codebook management. This project combines both into a single library with a clean API and a CLI tool that handles everything automatically.

## Installation

### Prebuilt CLI Binaries

Prebuilt `wwtools` binaries for Windows, macOS, and Linux are available on the [GitHub Releases](https://github.com/tnt-coders/wwise-audio-tools/releases) page: a ZIP archive for each platform, plus a Windows installer, a Linux `.deb` package and a macOS disk image. Each holds `wwtools` in a `bin` folder (on Windows beside the Microsoft C++ runtime it needs); add that folder to your `PATH`. The Windows installer installs to `Program Files\TNT Music Studios\Wwise Audio Tools`.

### Library Integration

This package is not on Conan Center. To consume it as a dependency, use the [tnt-coders fork of cmake-conan](https://github.com/tnt-coders/cmake-conan) which supports building packages from source via a `#recipe:` annotation.

In your `conanfile.txt`:

```ini
[requires]
wwise-audio-tools/1.0.3 #recipe: https://github.com/tnt-coders/wwise-audio-tools.git

[generators]
CMakeDeps
CMakeToolchain
```

Or in your `conanfile.py`:

```python
def requirements(self):
    self.requires("wwise-audio-tools/1.0.3") #recipe: https://github.com/tnt-coders/wwise-audio-tools.git
```

The cmake-conan provider will automatically clone the repository, run `conan create`, and cache the package. No manual steps are needed.

In your `CMakeLists.txt`:
```cmake
find_package(WwiseAudioTools REQUIRED)
target_link_libraries(your_target PRIVATE WwiseAudioTools::WwiseAudioTools)
```

## Usage

### Command-Line Tool

```bash
# Convert all WEM files in the current directory
./wwtools

# Convert a specific WEM file to OGG
./wwtools wem input.wem

# Get WEM file metadata
./wwtools wem input.wem --info

# Extract and convert WEMs from a BNK soundbank
./wwtools bnk extract soundbank.bnk

# Extract raw WEMs without converting to OGG (written to a soundbank/ subdirectory)
./wwtools bnk extract soundbank.bnk --no-convert

# Get BNK soundbank info (version, embedded WEM IDs)
./wwtools bnk soundbank.bnk --info

# Show event-to-WEM mappings (all events or a specific event ID)
./wwtools bnk event soundbank.bnk
./wwtools bnk event soundbank.bnk 12345
```

When extracting from a BNK, streamed WEMs (those not fully embedded) require the corresponding `<id>.wem` file to be present in the same directory as the BNK.

### Library API

The public API is defined in a single header: `wwtools/wwtools.h`

**WEM to OGG conversion:**
```cpp
#include "wwtools/wwtools.h"
#include <fstream>
#include <sstream>

// Read WEM file
std::ifstream input("audio.wem", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();

// Convert to OGG (throws on failure)
std::string ogg_data = wwtools::Wem2Ogg(buffer.str());

std::ofstream output("audio.ogg", std::ios::binary);
output << ogg_data;
```

**Extracting WEMs from a BNK soundbank:**
```cpp
#include "wwtools/wwtools.h"
#include <fstream>
#include <sstream>

// Read BNK file
std::ifstream input("soundbank.bnk", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();

// Extract all WEMs with their IDs and streaming status
std::vector<wwtools::BnkEntry> wems = wwtools::BnkExtract(buffer.str());

for (const auto& wem : wems) {
    if (wem.streamed) {
        // Load full audio from external <wem.id>.wem file
        continue;
    }

    // Convert embedded WEM data to OGG
    std::string ogg_data = wwtools::Wem2Ogg(wem.data);

    std::ofstream output(std::to_string(wem.id) + ".ogg", std::ios::binary);
    output << ogg_data;
}
```

## Building from Source

### Requirements

- CMake 3.27 or higher
- [Ninja](https://ninja-build.org/), the presets' generator
- C++23 compatible compiler
- [Conan](https://conan.io/) 2.0.5 or higher, and Git

Dependencies are installed automatically via [cmake-conan](https://github.com/tnt-coders/cmake-conan) during the CMake configure step. It lives in the `project-config` submodule, so check that out, with its own submodules, first:

```bash
git submodule update --init --recursive
```

```bash
# Configure (automatically installs Conan dependencies)
cmake --preset release

# Build
cmake --build --preset release

# Run tests
ctest --preset release
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_CLI` | `ON` | Build the `wwtools` command-line tool |
| `PACKED_CODEBOOKS_AOTUV` | `ON` | Use aoTuV 603 codebook data (recommended) |
| `BUILD_TESTING` | `ON` | Build the unit tests |
| `PROJECT_CONFIG_ENABLE_DOCS` | `ON` | Build the documentation with every build (requires Doxygen) |
| `PROJECT_CONFIG_ENABLE_CLANG_TIDY` | `ON` | Enable clang-tidy lint targets (requires clang-tidy) |

### Linting

Requires [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) and `run-clang-tidy` to be installed (typically from an LLVM/Clang package). The targets are only available when `run-clang-tidy` is found on `PATH`.

```bash
cmake --build --preset release --target clang-tidy       # check only
cmake --build --preset release --target clang-tidy-fix   # auto-fix
```

### Documentation

API documentation for the latest stable release is hosted on GitHub Pages at:
https://tnt-coders.github.io/wwise-audio-tools/

To build the documentation locally for a specific release or branch, you need [Doxygen](https://www.doxygen.nl/) 1.9.6 or higher on `PATH`, and Python 3. When Doxygen is found, every build generates the documentation; the `docs` target builds it alone:

```bash
git checkout <tag-or-branch>
cmake --preset release
cmake --build --preset release --target docs
```

The generated HTML will be in `build/release/docs/html/`.
