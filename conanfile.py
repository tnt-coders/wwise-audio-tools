from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, collect_libs
import os


class WwiseAudioToolsConan(ConanFile):
    name = "wwtools"
    version = "1.0.0"
    license = "MIT"
    author = "TNT Coders"
    url = "https://github.com/tnt-coders/wwise-audio-tools"
    description = "Library and command line tool for converting Wwise WEM files to OGG format"
    topics = ("wwise", "audio", "wem", "ogg", "conversion")

    # Binary configuration
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

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*"

    def config_options(self):
        """Remove options not available on certain platforms"""
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        """Configure options based on settings"""
        if self.options.shared:
            # Shared library doesn't need fPIC
            self.options.rm_safe("fPIC")

    def requirements(self):
        """Declare dependencies"""
        self.requires("ogg/1.3.5")
        self.requires("vorbis/1.3.7")

    def layout(self):
        """Define the layout for the build"""
        cmake_layout(self)

    def generate(self):
        """Generate the toolchain and dependency files"""
        # CMakeToolchain for build configuration
        tc = CMakeToolchain(self)

        # Pass options to CMake
        tc.variables["PACKED_CODEBOOKS_AOTUV"] = self.options.packed_codebooks_aotuv

        # Disable tests for Conan build
        tc.variables["DOWNLOAD_CATCH2"] = False

        # Generate the toolchain file
        tc.generate()

        # CMakeDeps to find dependencies
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        """Build the project using CMake"""
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        """Package the built artifacts"""
        cmake = CMake(self)
        cmake.install()

        # Copy license if it exists
        copy(self, "LICENSE*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "COPYING*", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        """Define how consumers will use this package"""
        # This package uses its own CMake config files (wwtoolsConfig.cmake)
        # Conan will use the installed CMake config files for find_package()
        self.cpp_info.set_property("cmake_find_mode", "both")
        self.cpp_info.set_property("cmake_file_name", "wwtools")
        self.cpp_info.set_property("cmake_target_name", "wwtools::static" if not self.options.shared else "wwtools::shared")

        # Define components for both static and shared builds
        # The actual linking will be handled by the project's CMake config files
        if self.options.shared:
            self.cpp_info.components["shared"].set_property("cmake_target_name", "wwtools::shared")
            self.cpp_info.components["shared"].libs = ["wwtools"]
            self.cpp_info.components["shared"].requires = ["ogg::ogg", "vorbis::vorbis"]
        else:
            self.cpp_info.components["static"].set_property("cmake_target_name", "wwtools::static")
            self.cpp_info.components["static"].libs = ["wwtools"]
            self.cpp_info.components["static"].requires = ["ogg::ogg", "vorbis::vorbis"]

        # Define include directories
        self.cpp_info.includedirs = ["include"]

        # Set library directories
        self.cpp_info.libdirs = ["lib"]

        # Set binary directories (for the wwtools executable)
        self.cpp_info.bindirs = ["bin"]
