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
- Dynamic range normalization
- Optional contrast enhancement
- Export of processed frames in `.pgm` format
- Automatic reconstruction of output video using FFmpeg
- Runtime performance metrics collection
- Modular architecture prepared for future SPI integration

---

## Pipeline Overview

```text
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

```text
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
    (excluded from repository)
```

---

## Packet Format

Each packet in the emulated stream contains:

```text
4 bytes   - packet header
160 bytes - thermal payload
```

Payload contains 80 thermal pixels encoded as 16-bit big-endian values.

Two packets reconstruct one image row:

```text
Packet 0 -> left half of row
Packet 1 -> right half of row
```

A complete frame consists of:

```text
4 segments
60 packets per segment
240 packets per frame
```

---

## Build

Compile processing pipeline:

```bash
gcc -I. app/main.c \
    app/app_pipeline.c \
    core/frame.c \
    input/vospi_parser.c \
    input/frame_reconstructor.c \
    input/source_emulator.c \
    processing/bad_pixels.c \
    processing/denoise.c \
    processing/normalize.c \
    processing/contrast.c \
    output/output_image.c \
    -o main
```

Compile video-to-stream converter:

```bash
gcc -I. -O2 -o video_to_stream video_to_stream.c
```

---

## Usage

Convert thermal video into packet stream:

```bash
./video_to_stream input.mp4 testdata/packets/stream.bin 8
```

Run processing pipeline:

```bash
./main
```

Processed frames are saved to:

```text
testdata/output_frames/
```

Rebuild processed video manually:

```bash
ffmpeg -y -framerate 8 \
    -i testdata/output_frames/frame_%06d.pgm \
    -c:v libx264 -pix_fmt yuv420p \
    testdata/result_video.mp4
```

---

## Processing Configuration

Default processing configuration:

```c
cfg.enable_bad_pixel_correction = 1;
cfg.enable_denoise = 1;
cfg.enable_normalize = 1;
cfg.enable_contrast = 0;
```

Spatial median filtering is used instead of temporal filtering to avoid ghosting artifacts in dynamic scenes.

---

## Runtime Metrics

The pipeline measures:

- packet read time
- pipeline processing time
- frame export time
- video reconstruction time
- effective processing FPS
- average processing time per frame
- packet and frame statistics

Example runtime output:

```text
Packets processed: XXXXX
Frames ready: XXXXX
Frames dropped: 0

Pipeline FPS: XX.XX
Avg pipeline/frame: X.XXX ms
```

---

## Implementation Notes

- The project uses an emulated VoSPI-inspired packet format.
- Frame reconstruction is independent from the packet source.
- The architecture allows future SPI-based integration.
- Processing modules are isolated from input transport logic.
- Intermediate frame export simplifies debugging and testing.
- The project is optimized for clarity and modularity rather than hardware-level performance.

---

## Current Limitations

- No real SPI communication
- No direct FLIR Lepton integration
- Simplified packet header format
- Desktop-only execution
- No DMA or RTOS support

---

## Future Work

- Real SPI packet acquisition
- ESP32 integration
- Real FLIR Lepton support
- DMA-based packet handling
- Real-time optimization
- Adaptive thermal filtering algorithms

---

## License

Educational and research use.