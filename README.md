# Wwise Audio Tools
This repository provides a static and dynamic library as well as a command line tool for converting Wwise WEM files to OGG format.

## About
Both `ww2ogg` and `revorb` are relatively old and cumbersome to use repetitively. This project aims to maintain support for these tools while making them easier to use and integrate into other applications.

## Requirements
- CMake 3.12 or higher
- C++17 compatible compiler
- Git (to clone submodules)
- Dependencies (included as submodules):
  - libogg
  - libvorbis

## Building

This project uses CMake and includes all dependencies as git submodules.

### From Command Line:

```bash
git clone --recursive https://github.com/tnt-coders/wwise-audio-tools
cd wwise-audio-tools

cmake -S . -B build
cmake --build build --config=Release
```

**Note:** The `--recursive` flag automatically clones the ogg and vorbis submodules. If you already cloned without it:
```bash
git submodule update --init --recursive
```

This will create:
- Command-line tool: `build/bin/wwtools` (or `build/bin/wwtools.exe` on Windows)
- Static library: `build/lib/libwwtools.a` (or `build/lib/wwtools.lib` on Windows)
- Shared library: `build/lib/libwwtools.so` (or `build/lib/wwtools.dll` on Windows)

### With Qt Creator IDE:

Just open the main `CMakeLists.txt` file as a project and Qt Creator will handle the rest.

## Usage

### Command Line Tool:
The command-line tool can be run with `./wwtools [NAME].wem` which will generate an easily usable `[NAME].ogg` in the same directory. No need to use `revorb`, no need to have a `packed_codebooks.bin`.

For more advanced usage:
```bash
# Convert all WEM files in current directory
./wwtools

# Convert a specific WEM file
./wwtools wem input.wem

# Get WEM file information without converting
./wwtools wem input.wem --info

# Extract WEM files from a Soundbank (BNK) file
./wwtools bnk extract soundbank.bnk

# Extract WEM files without converting to OGG
./wwtools bnk extract soundbank.bnk --no-convert

# Find event information in a BNK file
./wwtools bnk event soundbank.bnk <event_id>

# Get BNK file information
./wwtools bnk extract soundbank.bnk --info

# Extract from Witcher 3 sound cache files
./wwtools cache read soundcache.cache
./wwtools cache write /path/to/directory

# Get cache file information
./wwtools cache read soundcache.cache --info
```

### Library:

The library provides both static and shared builds (`wwtools-static-lib` and `wwtools-shared-lib`). All public API functions are in the `wwtools`, `wwtools::bnk`, `wwtools::w3sc`, and `ww2ogg` namespaces.

#### Integrating into Your CMake Project

This library includes all dependencies as submodules, so integration is simple:

**Option 1: Git Submodule (Recommended)**

```bash
git submodule add --recursive https://github.com/tnt-coders/wwise-audio-tools external/wwise-audio-tools
```

In your CMakeLists.txt:
```cmake
add_subdirectory(external/wwise-audio-tools)
target_link_libraries(your_target PRIVATE wwtools::static)
```

**Option 2: FetchContent**

```cmake
include(FetchContent)

FetchContent_Declare(
    wwise-audio-tools
    GIT_REPOSITORY https://github.com/tnt-coders/wwise-audio-tools
    GIT_TAG master
    GIT_SUBMODULES_RECURSE ON
)

FetchContent_MakeAvailable(wwise-audio-tools)
target_link_libraries(your_target PRIVATE wwtools::static)
```

**That's it!** All dependencies are included and will be built automatically.

**Available Targets:**

- `wwtools::static` - Static library (recommended)
- `wwtools::shared` - Shared/dynamic library
- Legacy names `wwtools-static-lib` and `wwtools-shared-lib` also work

**Complete Example:**

```cmake
cmake_minimum_required(VERSION 3.12)
project(MyProject)

# Add wwise-audio-tools as submodule
add_subdirectory(external/wwise-audio-tools)

# Create your executable
add_executable(my_app main.cpp)

# Link - dependencies are automatic!
target_link_libraries(my_app PRIVATE wwtools::static)
```

#### API Reference

**Basic WEM to OGG Conversion:**
```cpp
#include "wwtools/wwtools.hpp"
#include <fstream>
#include <sstream>

// Read WEM file
std::ifstream input("audio.wem", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string wem_data = buffer.str();

// Convert WEM to OGG
std::string ogg_data = wwtools::wem_to_ogg(wem_data);

// Write OGG file
std::ofstream output("audio.ogg", std::ios::binary);
output << ogg_data;
```

**Working with Soundbank (BNK) Files:**
```cpp
#include "wwtools/bnk.hpp"
#include <fstream>
#include <sstream>
#include <vector>

// Read BNK file
std::ifstream input("soundbank.bnk", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string bnk_data = buffer.str();

// Extract all WEM files from the BNK
std::vector<std::string> wem_files;
wwtools::bnk::extract(bnk_data, wem_files);

// Convert each WEM to OGG
for (size_t i = 0; i < wem_files.size(); i++) {
    std::string ogg_data = wwtools::wem_to_ogg(wem_files[i]);

    // Get WEM ID for naming
    std::string wem_id = wwtools::bnk::get_wem_id_at_index(bnk_data, i);

    std::ofstream output(wem_id + ".ogg", std::ios::binary);
    output << ogg_data;
}

// Get BNK information
std::string info = wwtools::bnk::get_info(bnk_data);
std::cout << info << std::endl;

// Get event information
std::string event_info = wwtools::bnk::get_event_id_info(bnk_data, "12345");
std::cout << event_info << std::endl;
```

**Working with Sound Cache (W3SC) Files:**
```cpp
#include "wwtools/w3sc.hpp"
#include <fstream>
#include <sstream>
#include <vector>

// Read cache file
std::ifstream input("soundcache.cache", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string cache_data = buffer.str();

// Get cache information
std::string info = wwtools::w3sc::get_info(cache_data);
std::cout << info << std::endl;

// Create a new cache file
std::vector<std::pair<std::string, std::string>> files;
files.push_back({"file1.bnk", file1_data});
files.push_back({"file2.wem", file2_data});

std::ofstream output("newcache.cache", std::ios::binary);
wwtools::w3sc::create(files, output);
```

**Low-level WW2OGG API:**
```cpp
#include "ww2ogg/ww2ogg.h"
#include <fstream>
#include <sstream>

// Read WEM file
std::ifstream input("audio.wem", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string wem_data = buffer.str();

// Convert using low-level API
std::stringstream ogg_output;
bool success = ww2ogg::ww2ogg(wem_data, ogg_output);

if (success) {
    std::ofstream output("audio.ogg", std::ios::binary);
    output << ogg_output.str();
}

// Get WEM information
std::string info = ww2ogg::wem_info(wem_data);
std::cout << info << std::endl;
```

#### Required Headers

- `wwtools/wwtools.hpp` - Main WEM to OGG conversion
- `wwtools/bnk.hpp` - Soundbank file handling
- `wwtools/w3sc.hpp` - Sound cache file handling
- `ww2ogg/ww2ogg.h` - Low-level conversion functions

All header files are located in the `include/` directory.

## Credits
Credit for the initial version of this library goes to the developer team who created the repository this was forked from: https://github.com/WolvenKit/wwise-audio-tools

Credit for the `ww2ogg` code goes to [`@hcs64`](https://github.com/hcs64), and to [`Jiri Hruska`](https://hydrogenaud.io/index.php/topic,64328.0.html) for the creation of the original `revorb`.

## Licensing
Everything else is licensed under MIT, but explicit credit is required to be given to the original developers in the license file.
