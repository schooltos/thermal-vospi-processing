# Thermal VoSPI-Like Processing Pipeline

This project implements a thermal image processing pipeline for packet-based thermal frame reconstruction and embedded thermal imaging experiments.

The system emulates a FLIR Lepton-like VoSPI packet stream using prerecorded thermal video data, reconstructs thermal frames from segmented packets, applies image processing algorithms, and generates processed output video.

The project was developed as a software prototype for studying thermal frame reconstruction and image enhancement techniques without requiring physical FLIR Lepton hardware.

---

## Features

- Conversion of thermal video into packetized binary stream
- Emulation of Lepton-like packet transmission
- VoSPI-inspired packet parser
- Thermal frame reconstruction from segmented packet stream
- Bad pixel correction
- Spatial denoising using median 3x3 filtering
- Dynamic range normalization (min-max or percentile-based)
- Optional contrast enhancement (histogram equalization)
- Configurable processing profiles (default, people, transport, nature, drone)
- Export of processed frames in `.pgm` format
- Automatic reconstruction of output video using FFmpeg
- Automatic creation of output directories
- Runtime performance metrics collection (wall-clock based)
- Modular architecture prepared for future SPI integration

---

## Pipeline Overview

```
Video
   ↓
stream.bin
   ↓
Packet Emulator
   ↓
VoSPI-like Packet Parser
   ↓
Frame Reconstruction
   ↓
Image Processing Pipeline
   ↓
Processed Frames
   ↓
Output Video
```

---

## Project Structure

```
app/
    Pipeline orchestration and application logic

core/
    Shared types, constants and frame utilities

input/
    Packet emulator
    VoSPI-like parser
    Frame reconstruction logic

processing/
    Thermal image processing algorithms

output/
    Frame export utilities

testdata/
    Input videos, packet streams and generated output
    (excluded from repository; created automatically at runtime)
```

---

## Packet Format

Each packet in the emulated stream contains:

```
4 bytes   - packet header
160 bytes - thermal payload
```

Payload contains 80 thermal pixels encoded as 16-bit big-endian values.

Two packets reconstruct one image row:

```
Packet 0 -> left half of row
Packet 1 -> right half of row
```

A complete frame consists of:

```
4 segments
60 packets per segment
240 packets per frame
```

> **Note:** the header format is a simplified emulation (segment + packet
> number only) and has not been verified against the real FLIR Lepton
> VoSPI specification (discard-packet markers, CRC, telemetry rows are
> not currently modeled). See open issues for details.

---

## Build

The project uses a plain `Makefile`. From the repository root:

```
make
```

This builds two executables in the repository root:

- `main` — the processing pipeline
- `video_to_stream` — the video-to-packet-stream converter

Other useful targets:

```
make debug      # unoptimized build with debug symbols
make sanitize   # build with AddressSanitizer + UndefinedBehaviorSanitizer
make clean      # remove built binaries
```

Override the compiler or flags if needed:

```
make CC=clang CFLAGS="-Wall -Wextra -O3 -I."
```

---

## Usage

Convert thermal video into a packet stream:

```
./video_to_stream input.mp4 testdata/packets/stream.bin 8
```

Arguments: `<input_video> <output_stream> [fps]` (fps defaults to 8 if omitted).
The parent directory of `<output_stream>` is created automatically if it
doesn't exist.

Run the processing pipeline:

```
./main testdata/packets/stream.bin testdata/output_frames testdata/result_video.mp4
```

Arguments: `<stream.bin> <output_frames_dir> <output_video.mp4> [profile]`

The output frames directory is created automatically if it doesn't exist.
The pipeline also invokes FFmpeg internally to rebuild the output video —
no manual FFmpeg step is required.

### Processing profiles

The optional fourth argument selects a processing profile tuned for
different scene types:

| Profile     | Contrast | Percentile range | Notes                                   |
|-------------|----------|-------------------|------------------------------------------|
| `default`   | off      | 1–99%             | balanced, general-purpose                |
| `people`    | off      | 1–99%             | reduces risk of overexposing hot silhouettes |
| `transport` | off      | 2–98%             | balanced normalization for technical objects |
| `nature`    | on       | 1–99%             | boosts weak contrast in natural scenes   |
| `drone`     | on       | 2–98%             | emphasizes small objects on complex backgrounds |

Example:

```
./main testdata/packets/stream.bin testdata/output_frames testdata/result_video.mp4 drone
```

Processed frames are saved to the directory given as the second argument
(e.g. `testdata/output_frames/`), and the reconstructed video to the path
given as the third argument.

---

## Runtime Metrics

The pipeline measures (using wall-clock time via `CLOCK_MONOTONIC`):

- packet read time
- pipeline processing time
- frame export time
- video reconstruction time (including the external FFmpeg call)
- effective processing FPS
- average processing time per frame
- packet and frame statistics

Example runtime output:

```
========== Runtime metrics ==========
Packets read:        68160
Packets processed:   68160
Frames ready:        284
Frames written:      284
Frames dropped:      0

---------- Time ----------
Total time:          0.628637 s
Packet read time:    0.006030 s
Pipeline time:       0.448190 s
Frame output time:   0.026697 s
Video build time:    0.144004 s

---------- Performance ----------
Total FPS:           451.77 frames/s
Pipeline FPS:        633.66 frames/s
Avg pipeline/frame:  1.578 ms
Avg read/packet:     0.088 us
=====================================
```

---

## Implementation Notes

- The project uses an emulated VoSPI-inspired packet format.
- Frame reconstruction is independent from the packet source.
- The architecture allows future SPI-based integration.
- Processing modules are isolated from input transport logic.
- Intermediate frame export simplifies debugging and testing.
- The project is optimized for clarity and modularity rather than hardware-level performance.
- Output directories are created automatically by both `main` and `video_to_stream`.
- Runtime metrics use wall-clock (`CLOCK_MONOTONIC`) time so that time spent waiting on external processes (e.g. the FFmpeg subprocess for video reconstruction) is accounted for correctly.

---

## Current Limitations

- No real SPI communication
- No direct FLIR Lepton integration
- Simplified packet header format (see note under Packet Format)
- Desktop-only execution
- No DMA or RTOS support
- No automated tests or CI
- Limited handling of malformed/edge-case input (missing files, truncated streams)

---

## Future Work

- Real SPI packet acquisition
- ESP32 integration
- Real FLIR Lepton support
- DMA-based packet handling
- Real-time optimization
- Adaptive thermal filtering algorithms
- Unit tests and CI pipeline
- Discard-packet and stream resynchronization handling

---

## License

Educational use.
