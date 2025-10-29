# umx.cpp MSVC Build Documentation

This document details the build configuration and compilation process for the umx.cpp project on Windows x64 with Visual Studio 2022, including all modifications required for successful MSVC compilation.

## Project Overview

umx.cpp is a C++17 implementation of Open-Unmix (UMX), a neural network for music demixing. This build includes:

- **Quantized weights**: UMX-L weights compressed from 425 MB to 45 MB using uint8/uint16 quantization
- **Segmented inference**: Overlapping segment processing borrowed from Demucs
- **Streaming LSTM**: Optimized LSTM implementation for chunk-based processing
- **Wiener filtering**: Post-processing for improved separation quality

## MSVC Compilation Changes

This build required several modifications to the original codebase to ensure compatibility with the Microsoft Visual C++ (MSVC) compiler and Windows DLL export/import mechanisms. The following sections detail these changes.

### 1. Windows DLL Export/Import Support

**New File: [`src/export.hpp`](src/export.hpp)**

Created a new header file to handle Windows DLL symbol visibility using `__declspec(dllexport)` and `__declspec(dllimport)` directives:

```cpp
#ifndef EXPORT_HPP
#define EXPORT_HPP

// Define export/import macros for Windows DLL
#ifdef _WIN32
    #ifdef umx_cpp_lib_EXPORTS
        #define UMX_API __declspec(dllexport)
    #else
        #define UMX_API __declspec(dllimport)
    #endif
#else
    #define UMX_API
#endif

#endif // EXPORT_HPP
```

**Purpose**: On Windows, when building a shared library (DLL), functions and classes must be explicitly marked for export. This macro system:
- Uses `__declspec(dllexport)` when building the library (`umx_cpp_lib_EXPORTS` is defined)
- Uses `__declspec(dllimport)` when using the library from external code
- Remains empty on non-Windows platforms for cross-platform compatibility

### 2. API Function Declarations

**Modified Files**: 
- [`src/dsp.hpp`](src/dsp.hpp:4) - Added `#include "export.hpp"` and `UMX_API` macros
- [`src/inference.hpp`](src/inference.hpp:4) - Added `#include "export.hpp"` and `UMX_API` macros
- [`src/lstm.hpp`](src/lstm.hpp:4) - Added `#include "export.hpp"` and `UMX_API` macros
- [`src/model.hpp`](src/model.hpp:4) - Added `#include "export.hpp"` and `UMX_API` macros

All public API functions were marked with the `UMX_API` macro to ensure proper symbol visibility:

**Example from [`dsp.hpp`](src/dsp.hpp:107-116)**:
```cpp
UMX_API Eigen::MatrixXf load_audio(std::string filename);
UMX_API void write_audio_file(const Eigen::MatrixXf &waveform, std::string filename);
UMX_API Eigen::Tensor3dXcf polar_to_complex(const Eigen::Tensor3dXf &magnitude,
                                    const Eigen::Tensor3dXf &phase);
UMX_API void stft(struct stft_buffers &stft_buf);
UMX_API void istft(struct stft_buffers &stft_buf);
```

**Example from [`model.hpp`](src/model.hpp:59)**:
```cpp
UMX_API bool load_umx_model(const std::string &model_dir, struct umx_model *model);
```

**Example from [`lstm.hpp`](src/lstm.hpp:19-25)**:
```cpp
UMX_API struct lstm_data create_lstm_data(int hidden_size, int seq_len);
UMX_API void umx_lstm_set_zero(struct lstm_data *data);
UMX_API Eigen::MatrixXf umx_lstm_forward(struct umx_model *model, int target,
                                 const Eigen::MatrixXf &input,
                                 struct lstm_data *data, int hidden_size);
```

**Example from [`inference.hpp`](src/inference.hpp:21-24)**:
```cpp
UMX_API std::vector<Eigen::MatrixXf>
umx_inference(struct umx_model &model, const Eigen::MatrixXf audio,
              struct umxcpp::stft_buffers reusable_stft_buf,
              std::array<struct umxcpp::lstm_data, 4> &streaming_lstm_data);
```

### 3. CMake Build System Updates

**Modified File**: [`CMakeLists.txt`](CMakeLists.txt)

Added MSVC-specific compiler flags and conditional compilation logic:

**Lines 13-26**: MSVC compiler flag configuration
```cmake
if(MSVC)
  # Remove any existing flags and set MSVC-specific ones
  string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "-Wall" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "-Wextra" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /DNDEBUG")
else()
  # GCC/Clang flags remain unchanged
  set(CMAKE_CXX_FLAGS "-Wall -Wextra")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native ...")
endif()
```

**Key Changes**:
- `/W4`: Warning level 4 (high warning sensitivity)
- `/EHsc`: Standard C++ exception handling model
- `/DNDEBUG`: Define NDEBUG for release builds
- Removed GCC-specific optimization flags (`-march=native`, `-ffast-math`, etc.) for MSVC

**Lines 53-57**: Conditional BLAS/LAPACK linking
```cmake
if(NOT MSVC)
  find_package(BLAS REQUIRED)
  find_package(LAPACK REQUIRED)
endif()
```

**Reason**: MSVC builds don't require external BLAS/LAPACK libraries as Eigen can use its internal implementations efficiently on Windows.

**Lines 70-76**: DLL export definition and conditional linking
```cmake
add_library(umx.cpp.lib SHARED ${SOURCES})
target_compile_definitions(umx.cpp.lib PRIVATE umx_cpp_lib_EXPORTS)
if(MSVC)
  target_link_libraries(umx.cpp.lib libnyquist zlibstatic)
else()
  target_link_libraries(umx.cpp.lib libnyquist ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES} lapacke zlibstatic)
endif()
```

**Key Points**:
- `umx_cpp_lib_EXPORTS` is defined when building the library, enabling `__declspec(dllexport)`
- MSVC builds link only against `libnyquist` and `zlibstatic`
- Non-MSVC builds additionally link against BLAS, LAPACK, and lapacke

### 4. Summary of Changes

| File | Change Type | Description |
|------|-------------|-------------|
| [`src/export.hpp`](src/export.hpp) | **New** | Windows DLL export/import macro definitions |
| [`src/dsp.hpp`](src/dsp.hpp) | Modified | Added `UMX_API` to public functions |
| [`src/inference.hpp`](src/inference.hpp) | Modified | Added `UMX_API` to public functions |
| [`src/lstm.hpp`](src/lstm.hpp) | Modified | Added `UMX_API` to public functions |
| [`src/model.hpp`](src/model.hpp) | Modified | Added `UMX_API` to public functions |
| [`CMakeLists.txt`](CMakeLists.txt) | Modified | MSVC-specific compiler flags and conditional linking |
| [`umx.cpp`](umx.cpp) | No changes | Main program remains platform-agnostic |

### 5. Technical Rationale

**Why DLL Export/Import is Required on Windows**:

Unlike Unix-like systems where symbols are exported by default, Windows DLLs require explicit symbol visibility declarations. Without `__declspec(dllexport)` and `__declspec(dllimport)`:
- The linker cannot find symbols when linking against the DLL
- Results in "unresolved external symbol" errors
- The executable cannot call functions from the shared library

**Cross-Platform Compatibility**:

The `UMX_API` macro approach ensures:
- Windows builds work correctly with DLL export/import
- Linux/macOS builds remain unaffected (macro expands to nothing)
- Single codebase maintains portability across platforms
- No `#ifdef _WIN32` scattered throughout implementation files

## Build Characteristics

### Version Information
- **Platform**: Windows x64
- **Compiler**: Microsoft Visual C++ (MSVC) from Visual Studio 2022
- **Build Type**: Release (optimized)
- **C++ Standard**: C++17
- **CMake Version**: 3.31.2

### Key Features
- **OpenMP Support**: Multi-threaded processing for improved performance
- **Shared Library**: `umx.cpp.lib.dll` - Core inference library
- **Main Executable**: `umx.cpp.main.exe` - Command-line interface for music demixing
- **Position Independent Code**: Enabled for better security and compatibility

### Compiler Flags (MSVC)
- `/W4` - Warning level 4
- `/EHsc` - Exception handling model
- `/DNDEBUG` - Release mode optimizations

## Prerequisites

### Required Software

1. **Visual Studio 2022**
   - Download from: https://visualstudio.microsoft.com/vs/
   - Required workloads:
     - Desktop development with C++
     - C++ CMake tools for Windows

2. **CMake** (version 3.5 or higher)
   - Download from: https://cmake.org/download/
   - Or install via Visual Studio Installer

3. **vcpkg** (C++ Package Manager)
   - Installation instructions below

4. **Git** (for cloning submodules)
   - Download from: https://git-scm.com/download/win

### Python Environment (for model conversion)

```bash
# Create Python environment (using conda/mamba)
conda create --name umxcpp python=3.10
conda activate umxcpp

# Install Python dependencies
python -m pip install -r ../scripts/requirements.txt
```

## Installation Steps

### 1. Install vcpkg

vcpkg is Microsoft's C++ package manager that simplifies dependency management on Windows.

```powershell
# Clone vcpkg repository
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Add vcpkg to PATH (optional but recommended)
# Add C:\vcpkg to your system PATH environment variable
```

### 2. Install Required Dependencies with vcpkg

The project requires the following dependencies:

```powershell
# Navigate to vcpkg directory
cd C:\vcpkg

# Install Eigen3 (linear algebra library)
.\vcpkg install eigen3:x64-windows

# Install OpenMP support (included with MSVC, but ensure it's enabled)
# OpenMP is automatically available with Visual Studio 2022

# Integrate vcpkg with Visual Studio
.\vcpkg integrate install
```

**Note**: The project uses vendored submodules for:
- `libnyquist` (audio I/O)
- `eigen` (linear algebra)
- `zlib` (compression)

These are included in the `vendor/` directory and don't need separate installation.

### 3. Clone the Repository

```powershell
# Clone with submodules
git clone --recurse-submodules https://github.com/sevagh/umx.cpp
cd umx.cpp

# If you already cloned without submodules, initialize them:
git submodule update --init --recursive
```

### 4. Prepare Model Files

Before building, you need to convert the PyTorch model weights to GGML format:

```powershell
# Activate Python environment
conda activate umxcpp

# Convert UMX-L model to GGML format
python .\scripts\convert-umx-pth-to-ggml.py --model=umxl .\ggml-umxl

# Compress the model file (optional but recommended)
# Using 7-Zip or built-in compression
gzip -k .\ggml-models\ggml-model-umxl-u8.bin
```

This creates a compressed model file (~45 MB) from the original PyTorch weights.

## Building on Windows x64 with Visual Studio 2022

### Prerequisites: Load MSVC x64 Environment

Before building, you **must** load the Visual Studio x64 development environment. You have two options:

**Option 1: Use x64 Native Tools Command Prompt (Recommended)**
- Open Start Menu
- Search for "x64 Native Tools Command Prompt for VS 2022"
- This automatically loads the MSVC x64 environment

**Option 2: Load environment in existing PowerShell/CMD**
```powershell
# Run this command to load the MSVC x64 environment
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

# Or for Professional/Enterprise editions:
"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
```

### Build Commands (One-Line)

Once the MSVC x64 environment is loaded, use this single command to build:

```powershell
# Navigate to project root and build in one command
cd C:\Users\YourUsername\source\repos\umx.cpp
mkdir build && cd build && cmake .. -G "Visual Studio 17 2022" -A x64 && cmake --build . --config Release --target umx.cpp.main -j 8
```

### Build Commands (Step-by-Step)

If you prefer to see each step:

```powershell
# Navigate to project root
cd C:\Users\YourUsername\source\repos\umx.cpp

# Create and enter build directory
mkdir build
cd build

# Configure with CMake (using Visual Studio 2022 generator)
cmake .. -G "Visual Studio 17 2022" -A x64

# Build the project in Release mode with parallel compilation (8 jobs)
cmake --build . --config Release --target umx.cpp.main -j 8

# Or build all targets
cmake --build . --config Release -j 8

# Or build specific targets individually
cmake --build . --config Release --target umx.cpp.lib -j 8
cmake --build . --config Release --target umx.cpp.main -j 8
```

### Build Options Explained

- `-G "Visual Studio 17 2022"`: Specifies the CMake generator for VS 2022
- `-A x64`: Sets the platform architecture to 64-bit
- `--config Release`: Builds optimized release version (much faster than Debug)
- `--target umx.cpp.main`: Builds only the main executable (and its dependencies)
- `-j 8`: Uses 8 parallel jobs for faster compilation (adjust based on your CPU cores)

## Build Output

After successful compilation, you'll find the following files in the `build/Release/` directory:

- **umx.cpp.lib.dll** - Shared library containing core inference logic
- **umx.cpp.lib.lib** - Import library for linking
- **umx.cpp.lib.exp** - Export file
- **umx.cpp.main.exe** - Main executable for music demixing

## Usage

### Basic Usage

```powershell
# Navigate to build directory
cd build\Release

# Run umx.cpp.main
.\umx.cpp.main.exe <model_file> <input_wav> <output_dir>

# Example:
.\umx.cpp.main.exe ..\..\ggml-models\ggml-model-umxl-u8.bin.gz input.wav output_dir
```

### Command Line Arguments

- `<model_file>`: Path to the GGML model file (`.bin` or `.bin.gz`)
- `<input_wav>`: Path to input audio file (WAV format)
- `<output_dir>`: Directory where separated tracks will be saved

### Output Files

The program generates 4 separated audio tracks:
- `target_0.wav` - Vocals
- `target_1.wav` - Drums
- `target_2.wav` - Bass
- `target_3.wav` - Other instruments

## Project Structure

```
build/
├── README.md                    # This file
├── umx.cpp.sln                  # Visual Studio solution
├── CMakeCache.txt               # CMake configuration cache
├── cmake_install.cmake          # CMake install script
├── Release/                     # Release build output
│   ├── umx.cpp.lib.dll         # Shared library
│   ├── umx.cpp.lib.lib         # Import library
│   └── umx.cpp.main.exe        # Main executable
├── CMakeFiles/                  # CMake generated files
└── *.vcxproj                    # Visual Studio project files
```

## Dependencies

### Vendored (Included as Submodules)

- **Eigen** (v3.4+) - Linear algebra library
- **libnyquist** - Audio I/O library
- **zlib** - Compression library

### System Dependencies

- **OpenMP** - Parallel processing (included with MSVC)
- **Windows SDK** - Required by Visual Studio

## Troubleshooting

### Common Issues

1. **CMake cannot find vcpkg packages**
   ```powershell
   # Ensure vcpkg is integrated
   cd C:\vcpkg
   .\vcpkg integrate install
   
   # Or specify toolchain file explicitly
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

2. **OpenMP not found**
   - Ensure you're using Visual Studio 2022 with C++ workload installed
   - OpenMP is included by default with MSVC

3. **Submodules not initialized**
   ```powershell
   git submodule update --init --recursive
   ```

4. **Build fails with "cannot open file 'Eigen/Dense'"**
   - Verify Eigen submodule is present in `vendor/eigen/`
   - Re-initialize submodules if needed

5. **DLL not found when running executable**
   - Ensure `umx.cpp.lib.dll` is in the same directory as `umx.cpp.main.exe`
   - Or add the Release directory to your PATH

### Performance Tips

- Use Release build for optimal performance (10-20x faster than Debug)
- Ensure OpenMP is enabled for multi-threaded processing
- Use compressed model files (.bin.gz) to reduce disk I/O
- Process audio in segments for large files (handled automatically)

## Additional Resources

- **Main Repository**: https://github.com/sevagh/umx.cpp
- **Open-Unmix PyTorch**: https://github.com/sigsep/open-unmix-pytorch
- **GGML Format**: https://github.com/ggerganov/ggml
- **vcpkg Documentation**: https://vcpkg.io/

## License

This project follows the same license as the main umx.cpp repository. See the LICENSE file in the project root for details.

## Contributing

For build-related issues or improvements:
1. Check existing issues on GitHub
2. Create a new issue with detailed build logs
3. Include system information (Windows version, VS version, CMake version)

---

**Last Updated**: 2025-10-29  
**Build System**: CMake 3.31.2  
**Compiler**: MSVC (Visual Studio 2022)  
**Platform**: Windows x64