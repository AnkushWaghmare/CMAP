#!/bin/bash

#
# CMAP Uninstallation Script
# 
# This script removes the Call Monitor and Analyzer Program (CMAP)
# from the system. It performs:
# - Privilege check
# - Binary removal
# - Status reporting
#
# Note: Must be run with root privileges (sudo)
#

# Verify root privileges before proceeding
if [ "$EUID" -ne 0 ]; then
    echo -e "\033[31mPlease run as root (use sudo)\033[0m"
    exit 1
fi

# Remove CMAP binary from system path
if [ -f "/usr/local/bin/cmap" ]; then
    rm /usr/local/bin/cmap
    if [ $? -eq 0 ]; then
        echo -e "\033[32mUninstallation complete!\033[0m"
    else
        echo -e "\033[31mUninstallation failed!\033[0m"
        exit 1
    fi
else
    echo -e "\033[33mcmap is not installed\033[0m"
fi