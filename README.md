# Thermal VoSPI Processing Pipeline

This project implements a complete thermal image processing pipeline designed for embedded systems and real-time thermal imaging applications. The system converts an input thermal video into a VoSPI-like packet stream, reconstructs frames from packetized data, applies image processing algorithms, exports processed frames, and rebuilds the final output video.

The project was developed as a modular software prototype for thermal image processing systems and is prepared for further integration with real thermal sensors such as FLIR Lepton.

## Features

- Thermal video to VoSPI-like `.bin` stream conversion
- Packet-based input emulation
- Frame reconstruction from packet stream
- Bad pixel correction
- Spatial denoising using median 3x3 filtering
- Dynamic range normalization
- Optional contrast enhancement
- Processed frame export in `.pgm` format
- Automatic reconstruction of processed video using FFmpeg
- Modular architecture prepared for future SPI/VoSPI input

## Pipeline Overview

```text
Video -> stream.bin -> Emulator -> Packet Processing -> Frame Reconstruction -> Image Processing -> Output Frames -> Video
```

## Project Structure

```text
app/            - pipeline orchestration and control logic
core/           - core data types and configuration
input/          - data sources and frame reconstruction modules
processing/     - image processing algorithms
output/         - frame export utilities
testdata/       - input/output data, excluded from repository
```

## Build

From the project root:

```bash
gcc -I. app/main.c \
    app/app_pipeline.c \
    core/frame.c \
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

Convert video to packet stream:

```bash
gcc -O2 -o video_to_stream video_to_stream.c
./video_to_stream input.mp4 testdata/packets/stream.bin 8
```

Run the processing pipeline:

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

The default configuration prioritizes stability and visual clarity for dynamic scenes:

```c
cfg.enable_bad_pixel_correction = 1;
cfg.enable_denoise = 1;
cfg.enable_normalize = 1;
cfg.enable_contrast = 0;
```

A spatial median 3x3 filter is used for denoising instead of temporal IIR filtering. This removes visible ghosting artifacts in moving scenes while still reducing image noise.

## Implementation Notes

- The system operates on a packetized data model compatible with a simplified VoSPI-like stream.
- Frame reconstruction is separated from the data source, so the same pipeline can work with both an emulator and future SPI input.
- The processing pipeline is designed for low-resource environments and can be adapted for ESP32-class devices.
- Intermediate `.pgm` frame export is used for debugging, analysis, and final video reconstruction.

## Results

The pipeline successfully processes thermal video streams, reconstructs frames from packet data, applies spatial denoising and normalization, and produces a stable grayscale output video without temporal ghosting artifacts.

## Future Work

- Real VoSPI/SPI input implementation
- ESP32 performance optimization
- Adaptive filtering based on scene dynamics
- DMA-based packet acquisition
- Hardware-level integration with FLIR Lepton

## License

Educational use.