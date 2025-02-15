#!/bin/bash

#
# CMAP Uninstallation Script
# 
# This script removes the Call Monitor and Analyzer Program (CMAP)
# from macOS systems. It performs:
# - Platform verification
# - Architecture detection
# - Binary and configuration removal
# - Man page cleanup
#

# Terminal colors for pretty output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

# Check if running on macOS
if [[ "$(uname)" != "Darwin" ]]; then
    echo -e "${RED}Error: This uninstaller only supports macOS${NC}"
    exit 1
fi

# Detect architecture and set binary path
ARCH=$(uname -m)
if [[ "$ARCH" == "arm64" ]]; then
    BIN_PATH="/opt/homebrew/bin"
else
    BIN_PATH="/usr/local/bin"
fi

# Define paths
CONF_PATH="$HOME/Library/Application Support/cmap"
MAN_PATH="/usr/local/share/man/man1"

echo "Uninstalling CMAP..."

# Remove binary
if [ -f "$BIN_PATH/cmap" ]; then
    sudo rm "$BIN_PATH/cmap"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Removed binary${NC}"
    else
        echo -e "${RED}✗ Failed to remove binary${NC}"
        exit 1
    fi
fi

# Remove configuration
if [ -d "$CONF_PATH" ]; then
    rm -rf "$CONF_PATH"
    echo -e "${GREEN}✓ Removed configuration${NC}"
fi

# Remove man page
if [ -f "$MAN_PATH/cmap.1.gz" ]; then
    sudo rm "$MAN_PATH/cmap.1.gz"
    echo -e "${GREEN}✓ Removed documentation${NC}"
fi

echo -e "${GREEN}Uninstallation complete!${NC}"