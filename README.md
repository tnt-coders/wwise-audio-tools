# Wwise Audio Tools
This repository provides a static and dynamic library as well as a command line tool for converting Wwise WEM files to OGG format.

## ⚠️ Active Development Notice

This project is under active development. The information in this README may not reflect the current state of the codebase. Please check the latest commits and issues for the most up-to-date information.

## About
Both `ww2ogg` and `revorb` are relatively old and cumbersome to use repetitively. This project maintains support for these tools while making them easier to use and integrate into other applications.

## Requirements
- CMake 3.12 or higher  
- C++17 compatible compiler  
- Git (to clone submodules)  

## Dependencies / Submodules
The project includes its dependencies as Git submodules:  
- `libogg`  
- `libvorbis`  

### Cloning with Submodules
Clone the repository **with** submodules:  
```bash
git clone --recursive https://github.com/tnt-coders/wwise-audio-tools
```

If you already cloned without `--recursive`:  
```bash
git submodule update --init --recursive
```

This ensures all dependencies are present before building.

## Building

### From Command Line
```bash
cd wwise-audio-tools
cmake -S . -B build
cmake --build build --config=Release
```

This will create:  
- Command-line tool: `build/bin/wwtools` (or `wwtools.exe` on Windows)  
- Static library: `build/lib/libwwtools.a` (or `wwtools.lib` on Windows)  
- Shared library: `build/lib/libwwtools.so` (or `wwtools.dll` on Windows)  

### With Qt Creator IDE
Open the main `CMakeLists.txt` file as a project; Qt Creator will handle the rest.

## Usage

### Command Line Tool
```bash
./wwtools [NAME].wem
```
This generates `[NAME].ogg` in the same directory, no need for `revorb` or `packed_codebooks.bin`.

Advanced usage:
```bash
# Convert all WEM files in current directory
./wwtools

# Convert a specific WEM file
./wwtools wem input.wem

# Get WEM file information
./wwtools wem input.wem --info

# Extract WEM files from a BNK
./wwtools bnk extract soundbank.bnk

# Extract WEM without converting to OGG
./wwtools bnk extract soundbank.bnk --no-convert

# Get BNK information
./wwtools bnk extract soundbank.bnk --info

# Find event information in a BNK file
./wwtools bnk event soundbank.bnk <event_id>

# Work with Witcher 3 cache files
./wwtools cache read soundcache.cache
./wwtools cache write /path/to/directory
./wwtools cache read soundcache.cache --info
```

### Library Integration
The library provides both static and shared builds (`wwtools-static-lib` and `wwtools-shared-lib`). Public APIs are in `wwtools`, `wwtools::bnk`, `wwtools::w3sc`, and `ww2ogg` namespaces.

#### Approach 1: Submodule (Recommended for Development)
```bash
git submodule add --recursive https://github.com/tnt-coders/wwise-audio-tools external/wwise-audio-tools
```

In `CMakeLists.txt`:
```cmake
add_subdirectory(external/wwise-audio-tools)
target_link_libraries(your_target PRIVATE wwtools::static)
```

##### Using FetchContent Alternative
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

#### Approach 2: Installed Library (Recommended for Production)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config=Release
cmake --install build --prefix /path/to/install
```

In your project:
```cmake
find_package(wwtools REQUIRED)
target_link_libraries(my_app PRIVATE wwtools::static)
```

Use `CMAKE_PREFIX_PATH` or `wwtools_DIR` if installed in a custom location:
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/install
# or
cmake -S . -B build -Dwwtools_DIR=/path/to/install/lib/cmake/wwtools
```

**Available Targets:**  
- `wwtools::static` (recommended)  
- `wwtools::shared`  
- Legacy names `wwtools-static-lib` and `wwtools-shared-lib` also work

### Library API Examples

**Basic WEM to OGG Conversion:**
```cpp
#include "wwtools/wwtools.hpp"
#include <fstream>
#include <sstream>

std::ifstream input("audio.wem", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string wem_data = buffer.str();

std::string ogg_data = wwtools::wem_to_ogg(wem_data);

std::ofstream output("audio.ogg", std::ios::binary);
output << ogg_data;
```

**Working with Soundbank (BNK) Files:**
```cpp
#include "wwtools/bnk.hpp"
#include <fstream>
#include <sstream>
#include <vector>

std::ifstream input("soundbank.bnk", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string bnk_data = buffer.str();

std::vector<std::string> wem_files;
wwtools::bnk::extract(bnk_data, wem_files);

for (size_t i = 0; i < wem_files.size(); i++) {
    std::string ogg_data = wwtools::wem_to_ogg(wem_files[i]);
    std::string wem_id = wwtools::bnk::get_wem_id_at_index(bnk_data, i);
    std::ofstream output(wem_id + ".ogg", std::ios::binary);
    output << ogg_data;
}

std::cout << wwtools::bnk::get_info(bnk_data) << std::endl;
std::cout << wwtools::bnk::get_event_id_info(bnk_data, "12345") << std::endl;
```

**Working with Sound Cache (W3SC) Files:**
```cpp
#include "wwtools/w3sc.hpp"
#include <fstream>
#include <sstream>
#include <vector>

std::ifstream input("soundcache.cache", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string cache_data = buffer.str();

std::cout << wwtools::w3sc::get_info(cache_data) << std::endl;

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

std::ifstream input("audio.wem", std::ios::binary);
std::stringstream buffer;
buffer << input.rdbuf();
std::string wem_data = buffer.str();

std::stringstream ogg_output;
bool success = ww2ogg::ww2ogg(wem_data, ogg_output);

if (success) {
    std::ofstream output("audio.ogg", std::ios::binary);
    output << ogg_output.str();
}

std::cout << ww2ogg::wem_info(wem_data) << std::endl;
```

