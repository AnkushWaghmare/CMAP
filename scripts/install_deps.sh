#!/bin/bash

# Check if brew is installed
if ! command -v brew &> /dev/null; then
    echo "Homebrew is not installed. Please install it first."
    exit 1
fi

# Install opus if not already installed
if ! brew list opus &> /dev/null; then
    echo "Installing Opus codec library..."
    brew install opus
else
    echo "Opus codec library is already installed"
fi

# Install pkg-config if not already installed
if ! command -v pkg-config &> /dev/null; then
    echo "Installing pkg-config..."
    brew install pkg-config
fi
