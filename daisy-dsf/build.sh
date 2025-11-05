#!/bin/bash
# Build script for daisy-dsf
# Usage: ./build.sh [clean|flash|all]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Project name
PROJECT="daisy-dsf"

# Print colored message
print_msg() {
    echo -e "${GREEN}[${PROJECT}]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check if required tools are installed
check_tools() {
    print_msg "Checking for required tools..."
    
    if ! command -v arm-none-eabi-gcc &> /dev/null; then
        print_error "arm-none-eabi-gcc not found. Install ARM GCC toolchain."
        echo "  macOS: brew install gcc-arm-embedded"
        echo "  Linux: sudo apt-get install gcc-arm-none-eabi"
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        print_error "make not found. Install build tools."
        exit 1
    fi
    
    print_msg "All required tools found ✓"
}

# Check if library directories exist
check_libs() {
    print_msg "Checking for required libraries..."
    
    if [ ! -d "../../libDaisy" ]; then
        print_error "libDaisy not found at ../../libDaisy"
        echo "Clone it with: git clone --recursive https://github.com/electro-smith/libDaisy.git"
        exit 1
    fi

    if [ ! -d "../../DaisySP" ]; then
        print_error "DaisySP not found at ../../DaisySP"
        echo "Clone it with: git clone --recursive https://github.com/electro-smith/DaisySP.git"
        exit 1
    fi

    if [ ! -d "../../kxmx_bluemchen" ]; then
        print_error "kxmx_bluemchen not found at ../../kxmx_bluemchen"
        echo "Clone it with: git clone --recursive https://github.com/recursinging/kxmx_bluemchen.git"
        exit 1
    fi
    
    print_msg "All required libraries found ✓"
}

# Build the project
do_build() {
    print_msg "Building ${PROJECT}..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    if [ $? -eq 0 ]; then
        print_msg "Build successful! ✓"
        print_msg "Binary: build/${PROJECT}.bin"
    else
        print_error "Build failed!"
        exit 1
    fi
}

# Clean build artifacts
do_clean() {
    print_msg "Cleaning build artifacts..."
    make clean
    print_msg "Clean complete ✓"
}

# Flash to hardware
do_flash() {
    print_msg "Preparing to flash..."
    
    if ! command -v dfu-util &> /dev/null; then
        print_error "dfu-util not found. Install it to flash firmware."
        echo "  macOS: brew install dfu-util"
        echo "  Linux: sudo apt-get install dfu-util"
        exit 1
    fi
    
    print_warning "Put your Daisy Seed in DFU mode:"
    print_warning "  1. Hold BOOT button"
    print_warning "  2. Press and release RESET"
    print_warning "  3. Release BOOT"
    read -p "Press Enter when ready..."
    
    print_msg "Flashing firmware..."
    make program-dfu
    
    if [ $? -eq 0 ]; then
        print_msg "Flash successful! ✓"
        print_msg "Your bluemchen should now be running DSF oscillator"
    else
        print_error "Flash failed!"
        exit 1
    fi
}

# Main script logic
main() {
    case "$1" in
        clean)
            check_tools
            check_libs
            do_clean
            ;;
        flash)
            check_tools
            check_libs
            if [ ! -f "build/${PROJECT}.bin" ]; then
                print_warning "Binary not found. Building first..."
                do_build
            fi
            do_flash
            ;;
        all)
            check_tools
            check_libs
            do_clean
            do_build
            do_flash
            ;;
        "")
            check_tools
            check_libs
            do_build
            ;;
        *)
            echo "Usage: $0 [clean|flash|all]"
            echo ""
            echo "Commands:"
            echo "  (none)  - Build the project"
            echo "  clean   - Clean build artifacts"
            echo "  flash   - Flash to hardware (builds if needed)"
            echo "  all     - Clean, build, and flash"
            exit 1
            ;;
    esac
}

main "$@"
