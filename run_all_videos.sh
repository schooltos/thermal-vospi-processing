#!/usr/bin/env bash

set -euo pipefail

RAW_DIR="testdata/raw_video"
PACKET_DIR="testdata/packets"
FRAME_BASE_DIR="testdata/output_frames"
RESULT_DIR="testdata/results"

FPS=8

mkdir -p "$RAW_DIR"
mkdir -p "$PACKET_DIR"
mkdir -p "$FRAME_BASE_DIR"
mkdir -p "$RESULT_DIR"

echo "Cleaning generated output folders..."

rm -f "$PACKET_DIR"/*.bin
rm -rf "$FRAME_BASE_DIR"/*
rm -f "$RESULT_DIR"/*.mp4
rm -f "$RESULT_DIR"/*.txt

echo "Cleanup completed."
echo

echo "Building video_to_stream..."
gcc -I. -O2 -o video_to_stream video_to_stream.c

echo "Building main pipeline..."
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

echo "Build completed."
echo

detect_profile()
{
    filename="$1"
    lower_name="$(echo "$filename" | tr '[:upper:]' '[:lower:]')"

    case "$lower_name" in
        *people*|*person*|*human*|*pedestrian*)
            echo "people"
            ;;

        *transport*|*vehicle*|*car*|*cars*|*road*|*traffic*)
            echo "transport"
            ;;

        *nature*|*animal*|*wildlife*|*forest*|*field*)
            echo "nature"
            ;;

        *drone*|*uav*|*aerial*)
            echo "drone"
            ;;

        *)
            echo "default"
            ;;
    esac
}

found_any=0

for video in "$RAW_DIR"/*.{mp4,mov,avi,mkv}; do
    if [ ! -f "$video" ]; then
        continue
    fi

    found_any=1

    filename="$(basename "$video")"
    name="${filename%.*}"

    profile="$(detect_profile "$filename")"

    stream_file="$PACKET_DIR/${name}_stream.bin"
    frame_dir="$FRAME_BASE_DIR/${name}"
    result_video="$RESULT_DIR/${name}_result.mp4"
    metrics_file="$RESULT_DIR/${name}_metrics.txt"

    echo "======================================"
    echo "Processing video: $filename"
    echo "Scene name:       $name"
    echo "Profile:          $profile"
    echo "======================================"

    echo "Cleaning frame directory..."
    rm -rf "$frame_dir"
    mkdir -p "$frame_dir"

    echo "Generating binary packet stream..."
    ./video_to_stream "$video" "$stream_file" "$FPS"

    echo "Running processing pipeline..."
    ./main "$stream_file" "$frame_dir" "$result_video" "$profile" | tee "$metrics_file"

    if [ -f "$result_video" ]; then
        echo "Saved processed video: $result_video"
    else
        echo "Warning: processed video was not created"
    fi

    echo "Saved metrics: $metrics_file"
    echo
done

if [ "$found_any" -eq 0 ]; then
    echo "No video files found in $RAW_DIR"
    echo "Supported formats: mp4, mov, avi, mkv"
    exit 1
fi

echo "All videos processed."