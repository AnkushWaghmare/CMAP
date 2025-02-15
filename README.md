<div align="center">

# 🎯 CMAP - Call Monitor and Analyzer Program

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-macOS-blue)]()

A real-time VoIP traffic monitoring and analysis tool that captures and analyzes SIP signaling and RTP media streams.

</div>

## ✨ Features

- 📡 Real-time packet capture from network interfaces
- 🔍 SIP signaling analysis and call state tracking
- 📊 RTP media stream monitoring
- 🎵 Audio quality enhancement using Opus codec
- 🛠️ Advanced packet loss concealment
- 🌐 NAT64 translation support
- ⚡ Adaptive jitter buffer management
- 💻 Command-line interface with various operation modes

## 📋 Requirements

- macOS operating system
- Homebrew package manager
- Command Line Tools for Xcode

## 🚀 Installation

Install the project:

<div class="code-block">
<pre><code class="language-bash">./script/install.sh</code></pre>
<button class="copy-button"></button>
</div>

Build the project:

<div class="code-block">
<pre><code class="language-bash">./scripts/build.sh</code></pre>
<button class="copy-button"></button>
</div>

The build script will automatically check and install all required dependencies using Homebrew.

## 💡 Usage

Basic usage:

<div class="code-block">
<pre><code class="language-bash">sudo ./cmap -i <interface> -O <output.pcap></code></pre>
<button class="copy-button"></button>
</div>

Example command:

<div class="code-block">
<pre><code class="language-bash">cmap -i en0 -a -d -O capture.pcap</code></pre>
<button class="copy-button"></button>
</div>



### Options:
| Option | Description |
|--------|-------------|
| `-i, --interface <if>` | Network interface to capture from |
| `-O, --output <file>` | Output file (pcap format) |
| `-t, --time <seconds>` | Stop after specified time |
| `-a, --auto` | Auto mode - stop when call ends |
| `-d, --debug` | Enable debug output |
| `-l, --list` | List available interfaces |
| `-s, --silent` | Suppress all output |
| `-h, --help` | Show help message |
| `-v, --version` | Show version information |

## 📁 Project Structure

```
src/
├── audio/    # Audio quality enhancement and Opus codec integration
├── cli/      # Command-line interface components
├── core/     # Core functionality and call session management
├── network/  # Network protocol handlers (RTP, SIP, packet capture)
└── utils/    # Utility functions and type definitions
```

## 🤝 Contributing

We welcome contributions! Please feel free to submit pull requests.

## 👥 Authors

- Ankush waghmare
- Deepak koli
- Aagam shah
- Kartik kokane 

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
