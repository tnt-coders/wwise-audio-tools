# Wwise Audio Tools
This repository provides a static and dynamic library as well as a command line tool for converting Wwise WEM files to OGG format.

## About
Both `ww2ogg` and `revorb` are relatively old and cumbersome to use repetitively. This project aims to maintain support for these tools while making them easier to use and integrate into other applications.

## Requirements
- CMake 3.12 or higher
- C++17 compatible compiler
- Conan package manager (for dependency management)
- Dependencies (automatically handled by Conan):
  - libogg
  - libvorbis

## Building
This project uses CMake to generate both static and dynamic libraries, as well as a command line tool.  


### With Qt Creator IDE:
If you have Qt Creator you should be able to just open the main `CMakeLists.txt` file as a project and select all default settings. Qt Creator should automatically run the Conan install step for you and everything should build out of the box. 

### From Command Line:

```bash
git clone https://github.com/tnt-coders/wwise-audio-tools
cd wwise-audio-tools

conan install . --build=missing
cmake -S . -B build
cmake --build build --config=Release
```

This will create:
- Command-line tool: `build/bin/wwtools` (or `build/bin/wwtools.exe` on Windows)
- Static library: `build/lib/libwwtools.a` (or `build/lib/wwtools.lib` on Windows)
- Shared library: `build/lib/libwwtools.so` (or `build/lib/wwtools.dll` on Windows)

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

#### Linking the Library

**CMake:**
```cmake
# Link against the shared library
target_link_libraries(your_target wwtools-shared-lib)

# Or link against the static library
target_link_libraries(your_target wwtools-static-lib)
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
