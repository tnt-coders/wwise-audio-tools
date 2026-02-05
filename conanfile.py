from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
import os


class WwiseAudioToolsConan(ConanFile):
    license = "MIT"
    author = "TNT Coders"
    url = "https://github.com/tnt-coders/wwise-audio-tools"
    description = "Library and command line tool for converting Wwise WEM files to OGG format"
    topics = ("wwise", "audio", "wem", "ogg", "conversion")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "packed_codebooks_aotuv": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "packed_codebooks_aotuv": True,
    }
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("cmake-package-builder/1.0.0") #recipe: https://github.com/tnt-coders/cmake-package-builder.git
        self.requires("catch2/3.12.0")
        self.requires("kaitai_struct_cpp_stl_runtime/0.11")
        self.requires("ogg/1.3.5")
        self.requires("rang/3.2")
        self.requires("vorbis/1.3.7")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["PACKED_CODEBOOKS_AOTUV"] = self.options.packed_codebooks_aotuv
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "config")
        self.cpp_info.set_property("cmake_file_name", "WwiseAudioTools")
        self.cpp_info.set_property("cmake_target_name", "WwiseAudioTools::WwiseAudioTools")
