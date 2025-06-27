#!/bin/bash

echo "🎵 EchoSub - Installing dependencies on macOS..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "✅ Homebrew found"
fi

# Update Homebrew
echo "🔄 Updating Homebrew..."
brew update

# Install Qt 6
echo "📦 Installing Qt 6..."
brew install qt6

# Install FFmpeg
echo "🎬 Installing FFmpeg..."
brew install ffmpeg

# Install CMake
echo "🔨 Installing CMake..."
brew install cmake

# Install build tools
echo "🛠️ Installing build tools..."
brew install make

echo "✅ All dependencies installed!"
echo ""
echo "Next steps:"
echo "1. mkdir build"
echo "2. cd build"
echo "3. cmake .."
echo "4. cmake --build ."
echo "5. ./bin/echosub" 