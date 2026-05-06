# Thermal VoSPI-Like Processing Pipeline

This project implements a thermal image processing pipeline designed for embedded-oriented systems and thermal imaging research. The system emulates a VoSPI-like packet stream inspired by FLIR Lepton transmission logic, reconstructs thermal frames from packetized data, applies image enhancement algorithms, and generates processed output video.

The project was developed as a software prototype for studying thermal frame reconstruction and image processing techniques without requiring physical thermal camera hardware. Instead of direct communication with a real FLIR Lepton sensor, the system uses prerecorded thermal video converted into an emulated VoSPI-like stream.

## Features

- Conversion of thermal video into packetized binary stream
- Emulation of Lepton-like VoSPI packet delivery
- Frame reconstruction from segmented packet stream
- Bad pixel correction
- Spatial denoising using median 3x3 filtering
- Dynamic range normalization
- Optional contrast enhancement
- Export of processed frames in `.pgm` format
- Automatic reconstruction of processed video using FFmpeg
- Modular architecture prepared for future SPI integration

## Pipeline Overview

```text
Video -> stream.bin -> Emulator -> VoSPI-like Packet Processing -> Frame Reconstruction -> Image Processing -> Output Frames -> Video
```

## Project Structure

```text
app/            - pipeline orchestration and application logic
core/           - shared types and configuration
input/          - emulator, packet parser and frame reconstruction
processing/     - image enhancement algorithms
output/         - frame export utilities
testdata/       - test streams and generated output (excluded from repository)
```

## Build

From the project root:

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

## Usage

Convert thermal video into packet stream:

```bash
gcc -O2 -o video_to_stream video_to_stream.c

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

Rebuild processed video:

```bash
ffmpeg -y -framerate 8 \
    -i testdata/output_frames/frame_%06d.pgm \
    -c:v libx264 -pix_fmt yuv420p \
    testdata/result_video.mp4
```

## Processing Configuration

Default processing configuration:

```c
cfg.enable_bad_pixel_correction = 1;
cfg.enable_denoise = 1;
cfg.enable_normalize = 1;
cfg.enable_contrast = 0;
```

Spatial median filtering is used instead of temporal IIR filtering to avoid visible ghosting artifacts in dynamic scenes.

## Implementation Notes

- The project uses an emulated VoSPI-like packet format rather than direct FLIR Lepton communication.
- Packet structure is based on segmented thermal frame transmission principles.
- Frame reconstruction is independent from the data source, allowing future SPI integration.
- Intermediate frame export simplifies debugging and algorithm evaluation.
- The processing pipeline is designed with embedded constraints in mind.

## Results

The implemented pipeline successfully reconstructs thermal frames from packetized data, applies spatial denoising and normalization, and generates stable processed thermal video without temporal ghosting artifacts.

## Current Limitations

- No real SPI/VoSPI hardware communication
- Simplified VoSPI-like header structure
- No DMA or hardware acceleration
- Processing currently runs on desktop environment

## Future Work

- Real SPI-based packet acquisition
- Integration with FLIR Lepton hardware
- ESP32 optimization
- Adaptive filtering strategies
- Real-time processing improvements

## License

Educational use.