# Wwise Audio Tools  
This repository is for a static and dynamic library as well as a simple command line tool used to convert Wwise WEM files to OGG files instead.

## About  
Both `ww2ogg` and `revorb` are relatively old and annoying to use repetitively so this project is aimed to keep supporting them and making them easy to use and integrate.

## Building
This is meant to be built with CMake, which generates both a static and dynamic library, as well as a simple POC command line tool.  


### With Qt Creator IDE:
If you have Qt Creator you should be able to just open the main `CMakeLists.txt` file as a project and select all default settings. Qt Creator should automatically run the Conan install step for you and everything should build out of the box. 

### From Command Line:

```
git clone https://github.com/tnt-coders/wwise-audio-tools

conan install . --build=missing
cmake -S . -B build
cmake --build build --config=Release
```  

This will create the command-line tool in the `bin/` directory and the libraries in `lib/`.

## Usage

### Command Line Tool:
The command-line tool can be run with `./wwise-audio-converter [NAME].wem` which will generate an easily usable `[NAME].ogg` in the same directory. No need to use `revorb`, no need to have a `packed_codebooks.bin`.

### Library:
The library usage will have further documentation soon.

TODO document examples of API and usage

## Credits
Credit for initial version of this library goes to the developer team who created the repository this forked from https://github.com/WolvenKit/wwise-audio-tools
Credit for the `ww2ogg` code goes to [`@hcs64`](https://github.com/hcs64), and to [`Jiri Hruska`](https://hydrogenaud.io/index.php/topic,64328.0.html) for the creation of the original `revorb`.

## Licensing
Everything else is licensed under MIT, but explicit credit is required to be given to the original developers in the license file.
