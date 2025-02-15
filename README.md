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

Build and install the project:

<div class="code-block">
<pre><code class="language-bash">./scripts/build.sh</code></pre>
<button class="copy-button" onclick="copyCode(this)">Copy</button>
</div>

The build script will automatically check and install all required dependencies using Homebrew.


## 💡 Usage

Basic usage:

<div class="code-block">
<pre><code class="language-bash">sudo ./cmap -i <interface> -O <output.pcap></code></pre>
<button class="copy-button" onclick="copyCode(this)">Copy</button>
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

1. Fork the repository
2. Create your feature branch:
<div class="code-block">
<pre><code class="language-bash">git checkout -b feature/amazing-feature</code></pre>
<button class="copy-button" onclick="copyCode(this)">Copy</button>
</div>

3. Commit your changes:
<div class="code-block">
<pre><code class="language-bash">git commit -m 'Add some amazing feature'</code></pre>
<button class="copy-button" onclick="copyCode(this)">Copy</button>
</div>

4. Push to the branch:
<div class="code-block">
<pre><code class="language-bash">git push origin feature/amazing-feature</code></pre>
<button class="copy-button" onclick="copyCode(this)">Copy</button>
</div>

5. Open a Pull Request

## 👥 Authors

- [Your names here]

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

<style>
.code-block {
	position: relative;
	margin: 10px 0;
}

.copy-button {
	position: absolute;
	top: 5px;
	right: 5px;
	padding: 5px 10px;
	background-color: #4CAF50;
	color: white;
	border: none;
	border-radius: 3px;
	cursor: pointer;
	opacity: 0.8;
}

.copy-button:hover {
	opacity: 1;
}

pre {
	background-color: #f6f8fa;
	padding: 16px;
	border-radius: 6px;
	overflow: auto;
}

table {
	width: 100%;
	border-collapse: collapse;
	margin: 20px 0;
}

th, td {
	padding: 12px;
	text-align: left;
	border-bottom: 1px solid #ddd;
}

th {
	background-color: #f6f8fa;
}
</style>

<script>
function copyCode(button) {
	const pre = button.parentElement.querySelector('pre');
	const code = pre.textContent;
	navigator.clipboard.writeText(code);
	
	button.textContent = 'Copied!';
	setTimeout(() => {
		button.textContent = 'Copy';
	}, 2000);
}
</script>