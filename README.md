# c64_tap_tool

`c64_tap_tool` is a command-line tool for handling C64 TAP files. It provides functionality to analyze, convert, and export data stored in the TAP format. The tool also supports converting PRG files to TAP and WAV formats.

## Features

- **Analyze TAP files**: Checks the structure and validity of TAP files.
- **Export PRG files**: Extracts PRG files from TAP files.
- **Convert PRG to TAP**: Creates TAP files from PRG files with correct pulse sequences.
- **Convert PRG to WAV**: Creates WAV files from PRG files with 44100 Hz, mono, and float data.
- **Support for C64 PAL frequencies**:
  - Short Pulse: 365.4 µs (2737 Hz, 360 cycles)
  - Medium Pulse: 531.4 µs (1882 Hz, 524 cycles)
  - Long Pulse: 697.6 µs (1434 Hz, 687 cycles)

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/username/c64_tap_tool.git
   cd c64_tap_tool
   ```

2. Build the project using CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. The executable will be located in the `build` directory.

## Usage

The tool supports various commands that can be executed from the command line:

### Commands

- **Show help**:
  ```bash
  ./c64_tap_tool --help
  ```

- **Show version**:
  ```bash
  ./c64_tap_tool --version
  ```

- **Analyze a TAP file**:
  ```bash
  ./c64_tap_tool --analyze <tap_filename>
  ```

- **Export PRG files from a TAP file**:
  ```bash
  ./c64_tap_tool --export <tap_filename>
  ```

- **Convert PRG to TAP**:
  ```bash
  ./c64_tap_tool --conv2tap <prg_filename> <tap_filename>
  ```

- **Convert PRG to WAV**:
  ```bash
  ./c64_tap_tool --conv2wav <prg_filename> <wav_filename>
  ```

## TAP File Format

The TAP format stores data as pulse sequences that correspond to the C64 loading sequences. The pulses are categorized into three types:
- **Short Pulse**: 365.4 µs
- **Medium Pulse**: 531.4 µs
- **Long Pulse**: 697.6 µs

The tool generates TAP files with correct pulse sequences, including header and data blocks.

## WAV File Format

The WAV files are created with the following properties:
- **Sample rate**: 44100 Hz
- **Channels**: Mono
- **Data format**: 32-bit Float

The pulse sequences are written to the WAV file as sine waves with the corresponding frequencies (e.g., 2737 Hz for Short Pulse).

## Examples

### Convert PRG to TAP
This command converts a PRG file into a TAP file with correct pulse sequences:
```bash
./c64_tap_tool --conv2tap example.prg example.tap
```

### Convert PRG to WAV
This command converts a PRG file into a WAV file with 44100 Hz, mono, and float data:
```bash
./c64_tap_tool --conv2wav example.prg example.wav
```

### Analyze a TAP file
This command analyzes the structure and validity of a TAP file:
```bash
./c64_tap_tool --analyze example.tap
```

### Export PRG files from a TAP file
This command extracts PRG files from a TAP file:
```bash
./c64_tap_tool --export example.tap
```

## Development

### Code Overview

- **`main.cpp`**: Main logic of the tool, including the implementation of commands.
- **Pulse Functions**:
  - `WriteTAPShortPulse`: Writes a Short Pulse to the TAP file.
  - `WriteTAPMediumPulse`: Writes a Medium Pulse to the TAP file.
  - `WriteTAPLongPulse`: Writes a Long Pulse to the TAP file.
  - `WriteTAPByte`: Converts a byte into pulse sequences and writes it to the TAP file.
  - `WriteWAVShortPulse`: Writes a Short Pulse as a sine wave to the WAV file.
  - `WriteWAVMediumPulse`: Writes a Medium Pulse as a sine wave to the WAV file.
  - `WriteWAVLongPulse`: Writes a Long Pulse as a sine wave to the WAV file.
  - `WriteWAVByte`: Converts a byte into sine wave pulse sequences and writes it to the WAV file.

### Requirements

- C++11 or newer
- CMake 3.5 or newer

## License

This project is licensed under the GNU GENERAL PUBLIC LICENSE. See the `LICENSE` file for details.

## Author

Created by Thorsten Kattanek. Feel free to open an issue in the repository for questions or suggestions.
```