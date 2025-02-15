#!/bin/bash

#
# CMAP Build Script
# 
# This script handles the compilation of the Call Monitor and Analyzer
# Program (CMAP). It provides:
# - Platform detection (macOS/Linux)
# - Architecture detection (Intel/ARM)
# - Dependency checking
# - Compiler flag configuration
# - Build process management
#
# Supported Platforms:
# - macOS (Intel and ARM)
# - Linux (x86_64)
#

# Detect system platform and architecture
PLATFORM=$(uname -s)
ARCH=$(uname -m)

# Compiler settings
CC=gcc
CFLAGS="-Wall -Wextra -g -O2"
LDFLAGS="-lpcap -lm"

# Add include paths for all source directories
CFLAGS="$CFLAGS -Isrc/core -Isrc/network -Isrc/audio -Isrc/cli -Isrc/utils"

# Add Opus paths
OPUS_VERSION="1.5.2"
OPUS_BASE="/opt/homebrew/Cellar/opus/$OPUS_VERSION"
OPUS_INCLUDE="-I$OPUS_BASE/include"
OPUS_LIB="-L$OPUS_BASE/lib -lopus"
CFLAGS="$CFLAGS $OPUS_INCLUDE"
LDFLAGS="$LDFLAGS $OPUS_LIB"

# Check if opus is installed
if [ ! -d "$OPUS_BASE" ]; then
    echo "Error: Opus library not found at $OPUS_BASE"
    echo "Please install opus using: brew install opus"
    exit 1
fi

# Source files
SOURCES="src/core/main.c \
         src/network/packet_utils.c \
         src/network/packet_capture.c \
         src/network/rtp_utils.c \
         src/network/sip_utils.c \
         src/core/call_session.c \
         src/cli/cli_interface.c \
         src/utils/debug.c \
         src/network/nat64_utils.c \
         src/audio/audio_quality.c"

# Detect system platform and architecture
PLATFORM=$(uname -s)
ARCH=$(uname -m)

# Configure Homebrew prefix based on architecture
# M1/ARM Macs use /opt/homebrew, Intel Macs use /usr/local
if [ "$PLATFORM" = "Darwin" ]; then
    if [ "$ARCH" = "arm64" ]; then
        BREW_PREFIX="/opt/homebrew"
    else
        BREW_PREFIX="/usr/local"
    fi
fi

echo -e "\033[36mChecking dependencies...\033[0m"

# Check if brew is installed
if ! command -v brew &> /dev/null; then
    echo -e "\033[31mError: Homebrew is not installed. Please install it first.\033[0m"
    echo "Install Homebrew using: /bin/bash -c '$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)'"
    exit 1
fi

# Function to check and install brew packages
install_if_missing() {
    local package=$1
    local description=$2
    
    if ! brew list $package &> /dev/null; then
        echo -e "\033[33m$description not found. Would you like to install it? (y/n)\033[0m"
        read -r response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])+$ ]]; then
            echo "Installing $description..."
            brew install $package
            if [ $? -ne 0 ]; then
                echo -e "\033[31mFailed to install $description\033[0m"
                exit 1
            fi
        else
            echo -e "\033[31m$description is required but was not installed\033[0m"
            exit 1
        fi
    else
        echo -e "\033[32m$description is already installed\033[0m"
    fi
}

# Check and install required packages
install_if_missing "opus" "Opus codec library"
install_if_missing "pkg-config" "Package configuration tool"
install_if_missing "libpcap" "Packet capture library"

# Verify GCC installation
if ! command -v gcc &> /dev/null; then
    echo -e "\033[33mGCC not found. Would you like to install Command Line Tools? (y/n)\033[0m"
    read -r response
    if [[ "$response" =~ ^([yY][eE][sS]|[yY])+$ ]]; then
        echo "Installing Command Line Tools..."
        xcode-select --install
    else
        echo -e "\033[31mGCC is required but was not installed\033[0m"
        exit 1
    fi
fi

# Set platform-specific compiler flags
# Different paths for libpcap based on platform and architecture
case "$PLATFORM" in
    "Darwin")
        if [ "$ARCH" = "arm64" ]; then
            # M1/ARM Mac configuration
            PCAP_CFLAGS="-I/opt/homebrew/opt/libpcap/include"
            PCAP_LDFLAGS="-L/opt/homebrew/opt/libpcap/lib"
        else
            # Intel Mac configuration
            PCAP_CFLAGS="-I/usr/local/opt/libpcap/include"
            PCAP_LDFLAGS="-L/usr/local/opt/libpcap/lib"
        fi
        ;;
    "Linux")
        # Linux uses standard system paths
        PCAP_CFLAGS=""
        PCAP_LDFLAGS=""
        ;;
    *)
        echo -e "\033[31mUnsupported platform: $PLATFORM\033[0m"
        exit 1
        ;;
esac

# Clean build artifacts
echo -e "\033[36mCleaning previous build...\033[0m"
rm -f docs/cmap

# Compile directly to executable
echo -e "\033[36mCompiling...\033[0m"
gcc $CFLAGS \
    $SOURCES \
    -o docs/cmap \
    $PCAP_CFLAGS $PCAP_LDFLAGS \
    $LDFLAGS

if [ $? -eq 0 ]; then
    echo -e "\033[32mBuild complete!\033[0m"
    echo -e "\033[36mRun 'sudo ./cmap -h' for usage\033[0m"
else
    echo -e "\033[31mBuild failed!\033[0m"
    exit 1
fi
