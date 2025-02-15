#!/bin/bash

#
# CMAP Installation Script
# 
# This script installs the Call Monitor and Analyzer Program (CMAP)
# on macOS systems. It handles:
# - Binary installation
# - Configuration file setup
# - Man page installation
# - Directory creation
# - Permission settings
#
# Installation Paths:
# - Binary: /usr/local/bin (Intel) or /opt/homebrew/bin (Apple Silicon)
# - Config: ~/Library/Application Support/cmap
# - Man Pages: /usr/local/share/man/man1
#

# Terminal color definitions for pretty output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Check if running on macOS
if [[ "$(uname)" != "Darwin" ]]; then
    echo "Error: This installer only supports macOS"
    exit 1
fi

# Detect architecture and set paths
ARCH=$(uname -m)
if [[ "$ARCH" == "arm64" ]]; then
    BIN_PATH="/opt/homebrew/bin"
else
    BIN_PATH="/usr/local/bin"
fi

# Define installation paths
CONF_PATH="$HOME/Library/Application Support/cmap"
MAN_PATH="/usr/local/share/man/man1"

echo -e "\n${CYAN}${BOLD}╭─ Installing CMAP ─────────────────╮${NC}"

# Create necessary directories
echo -e "${BLUE}│ Creating directories...${NC}"
mkdir -p "$BIN_PATH" "$CONF_PATH" "$MAN_PATH"

# Install the binary executable
echo -e "${BLUE}│ Installing binary...${NC}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cp "$PROJECT_ROOT/docs/cmap" "$BIN_PATH/"
chmod 755 "$BIN_PATH/cmap"

# Create and install default configuration
echo -e "${BLUE}│ Creating configuration...${NC}"
cat > "$CONF_PATH/cmap.conf" << EOF
# Default interface (common macOS network interfaces)
default_interface=en0

# Default capture settings
capture_timeout=300    # 5 minute default timeout
auto_mode=true        # Enable automatic mode by default
debug_mode=false      # Disable debug output by default

# RTP settings
rtp_timeout=30        # RTP stream timeout in seconds
max_streams=4         # Maximum concurrent RTP streams
EOF

chmod 644 "$CONF_PATH/cmap.conf"

# Install man page documentation
echo -e "${BLUE}│ Installing documentation...${NC}"
cat > "$MAN_PATH/cmap.1" << EOF
.TH CMAP 1 "$(date +"%B %Y")" "Version 1.0.1" "User Commands"

.SH NAME
       cmap \- VoIP Call Monitor and Analyzer for macOS

.SH SYNOPSIS
       cmap [\-i interface] [\-O output.pcap] [\-t seconds] [\-a] [\-d] [\-l] [\-s] [\-h] [\-v]

.SH DESCRIPTION
       Captures and analyzes VoIP calls using SIP and RTP protocols on macOS systems.

.SH OPTIONS
       \-i, \-\-interface <if>
              Network interface to capture from (e.g., en0, en1)

       \-O, \-\-output <file>
              Output file in PCAP format for packet capture

       \-t, \-\-time <seconds>
              Stop capturing after specified time in seconds

       \-a, \-\-auto
              Auto mode \- automatically stop when call ends

       \-d, \-\-debug
              Enable debug output for troubleshooting

       \-l, \-\-list
              List all available network interfaces

       \-s, \-\-silent
              Suppress all output (quiet mode)

       \-h, \-\-help
              Show this help message and command usage

       \-v, \-\-version
              Show version information

.SH EXAMPLES
       cmap \-i en0                    # Capture on interface en0
       cmap \-i en0 \-O capture.pcap    # Save to PCAP file
       cmap \-i en0 \-t 300            # Capture for 5 minutes
       cmap \-i en0 \-a \-d            # Auto mode with debug
       cmap \-i en0 \-s                # Silent capture

.SH FILES
       ~/Library/Application Support/cmap/cmap.conf    Configuration file
       ~/Library/Logs/cmap.log                        Debug log file

.SH ENVIRONMENT
       CMAP_DEBUG                      Set to 1 to enable debug mode
       CMAP_SILENT                     Set to 1 to enable silent mode

.SH AUTHOR
       Written by the CMAP Team

.SH COPYRIGHT
       Copyright \(co 2024 CMAP Team. MIT License.

.SH SEE ALSO
       wireshark(1), tcpdump(1), tshark(1)
EOF

chmod 644 "$MAN_PATH/cmap.1"
gzip -f "$MAN_PATH/cmap.1"

echo -e "${CYAN}╰────────────────────────────────────╯${NC}"
echo -e "\n${GREEN}${BOLD}Installation complete!${NC}"
echo -e "${BLUE}Configuration: ${NC}$CONF_PATH/cmap.conf"
echo -e "${BLUE}Run ${BOLD}'cmap -h'${NC}${BLUE} for usage information${NC}"
echo -e "${BLUE}Run ${BOLD}'man cmap'${NC}${BLUE} for detailed documentation${NC}\n"