#!/bin/bash

#
# CMAP Build Script
# 
# This script handles the compilation of the Call Monitor and Analyzer
# Program (CMAP). It provides:
# - Dependency installation
# - Architecture detection (Intel/ARM)
# - Compiler flag configuration
# - Build process management
#
# Supported Platforms:
# - macOS (Intel and ARM)
#

# Terminal colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Detect architecture
ARCH=$(uname -m)

echo -e "${CYAN}Checking dependencies...${NC}"

# Check if brew is installed
if ! command -v brew &> /dev/null; then
    echo -e "${RED}Error: Homebrew is not installed. Please install it first.${NC}"
    echo "Install Homebrew using: /bin/bash -c '$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)'"
    exit 1
fi

# Function to check and install brew packages
install_if_missing() {
    local package=$1
    local description=$2
    
    if ! brew list $package &> /dev/null; then
        echo -e "${YELLOW}$description not found. Would you like to install it? (y/n)${NC}"
        read -r response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])+$ ]]; then
            echo "Installing $description..."
            brew install $package
            if [ $? -ne 0 ]; then
                echo -e "${RED}Failed to install $description${NC}"
                exit 1
            fi
        else
            echo -e "${RED}$description is required but was not installed${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}$description is already installed${NC}"
    fi
}

# Install required packages
install_if_missing "opus" "Opus codec library"
install_if_missing "pkg-config" "Package configuration tool"
install_if_missing "libpcap" "Packet capture library"

# Verify GCC installation
if ! command -v gcc &> /dev/null; then
    echo -e "${YELLOW}GCC not found. Would you like to install Command Line Tools? (y/n)${NC}"
    read -r response
    if [[ "$response" =~ ^([yY][eE][sS]|[yY])+$ ]]; then
        echo "Installing Command Line Tools..."
        xcode-select --install
    else
        echo -e "${RED}GCC is required but was not installed${NC}"
        exit 1
    fi
fi

# Configure Homebrew prefix based on architecture
if [ "$ARCH" = "arm64" ]; then
    BREW_PREFIX="/opt/homebrew"
else
    BREW_PREFIX="/usr/local"
fi

# Compiler settings
CC=gcc
CFLAGS="-Wall -Wextra -g -O2"
LDFLAGS="-lpcap -lm"

# Add include paths for all source directories
CFLAGS="$CFLAGS -Isrc/core -Isrc/network -Isrc/audio -Isrc/cli -Isrc/utils"

# Add Opus paths based on architecture
OPUS_VERSION=$(brew list --versions opus | cut -d' ' -f2)
OPUS_BASE="$BREW_PREFIX/Cellar/opus/$OPUS_VERSION"
OPUS_INCLUDE="-I$BREW_PREFIX/opt/opus/include"
OPUS_LIB="-L$BREW_PREFIX/opt/opus/lib -lopus"
CFLAGS="$CFLAGS $OPUS_INCLUDE"
LDFLAGS="$LDFLAGS $OPUS_LIB"

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

# Set architecture-specific compiler flags
if [ "$ARCH" = "arm64" ]; then
    # M1/ARM Mac configuration
    PCAP_CFLAGS="-I/opt/homebrew/opt/libpcap/include"
    PCAP_LDFLAGS="-L/opt/homebrew/opt/libpcap/lib"
else
    # Intel Mac configuration
    PCAP_CFLAGS="-I/usr/local/opt/libpcap/include"
    PCAP_LDFLAGS="-L/usr/local/opt/libpcap/lib"
fi

# Clean build artifacts
echo -e "${CYAN}Cleaning previous build...${NC}"
rm -f docs/cmap

# Compile directly to executable
echo -e "${CYAN}Compiling...${NC}"
gcc $CFLAGS \
    $SOURCES \
    -o docs/cmap \
    $PCAP_CFLAGS $PCAP_LDFLAGS \
    $LDFLAGS

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build complete!${NC}"
    echo -e "${CYAN}Run 'sudo ./cmap -h' for usage${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

