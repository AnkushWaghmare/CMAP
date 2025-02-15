# CMAP - Call Monitor and Analyzer Program

CMAP is a real-time VoIP traffic monitoring and analysis tool that captures and analyzes SIP signaling and RTP media streams. It provides comprehensive call quality metrics and audio enhancement capabilities.

## Features

- Real-time packet capture from network interfaces
- SIP signaling analysis and call state tracking
- RTP media stream monitoring
- Audio quality enhancement using Opus codec
- Advanced packet loss concealment
- NAT64 translation support
- Adaptive jitter buffer management
- Command-line interface with various operation modes

## Requirements

- macOS or Linux operating system
- libpcap for packet capture
- Opus codec library
- pkg-config
- GCC or compatible C compiler

## Installation

1. Install dependencies:
```bash
# On macOS
brew install opus libpcap pkg-config

# On Linux
sudo apt-get install libopus-dev libpcap-dev pkg-config
```

2. Build the project:
```bash
./scripts/build.sh
```

## Usage

Basic usage:
```bash
sudo ./cmap -i <interface> -O <output.pcap>
```

Options:
- `-i, --interface <if>`: Network interface to capture from
- `-O, --output <file>`: Output file (pcap format)
- `-t, --time <seconds>`: Stop after specified time
- `-a, --auto`: Auto mode - stop when call ends
- `-d, --debug`: Enable debug output
- `-l, --list`: List available interfaces
- `-s, --silent`: Suppress all output
- `-h, --help`: Show help message
- `-v, --version`: Show version information

## Project Structure

- `src/audio/`: Audio quality enhancement and Opus codec integration
- `src/cli/`: Command-line interface components
- `src/core/`: Core functionality and call session management
- `src/network/`: Network protocol handlers (RTP, SIP, packet capture)
- `src/utils/`: Utility functions and type definitions

## Contributing

We welcome contributions! Please feel free to submit pull requests.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Authors

- [Your names here]

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.