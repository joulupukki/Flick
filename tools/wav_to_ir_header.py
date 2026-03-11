#!/usr/bin/env python3
"""
WAV to IR Header Converter

Converts WAV files (impulse responses) to C++ header format for embedding
in firmware. Originally from MuleBox, adapted for Flick.

IR data is placed in the .qspiflash_data section so it stays in QSPI
flash (memory-mapped read) rather than being copied to SRAM at boot.

Usage:
    python3 wav_to_ir_header.py [wav_files...] -o output.h

Example:
    python3 wav_to_ir_header.py src/irs/spring_reverb_48k.wav -o src/ir_data.h --max-length 1000
"""

import argparse
import cmath
import math
import wave
import struct
import os
import sys
from pathlib import Path

# Constants
SAMPLE_RATE = 48000
MAX_IR_LENGTH_MS = 1000  # Maximum IR length in milliseconds
MAX_IR_SAMPLES = int((MAX_IR_LENGTH_MS / 1000.0) * SAMPLE_RATE)  # 8,160 samples
MAX_IR_COUNT = 12


def read_wav_file(filepath):
    """
    Read a WAV file and return normalized float samples.

    Args:
        filepath: Path to WAV file

    Returns:
        tuple: (sample_rate, samples_list)

    Raises:
        ValueError: If WAV format is unsupported
    """
    with wave.open(str(filepath), 'rb') as wav:
        # Get WAV parameters
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        framerate = wav.getframerate()
        n_frames = wav.getnframes()

        print(f"  Format: {channels}ch, {sample_width*8}-bit, {framerate}Hz, {n_frames} samples ({n_frames/framerate*1000:.0f}ms)")

        # Read raw audio data
        raw_data = wav.readframes(n_frames)

        # Parse based on bit depth
        if sample_width == 2:  # 16-bit
            samples = struct.unpack(f'<{n_frames * channels}h', raw_data)
            max_val = 32768.0
        elif sample_width == 3:  # 24-bit
            # 24-bit samples are packed as 3 bytes each (all channels interleaved)
            samples = []
            for i in range(0, len(raw_data), 3):
                b1, b2, b3 = raw_data[i:i+3]
                # Combine bytes (little-endian, signed 24-bit)
                val = b1 | (b2 << 8) | (b3 << 16)
                # Sign extend from 24-bit to 32-bit
                if val & 0x800000:
                    val |= 0xFF000000
                # Convert to signed int32
                val = struct.unpack('<i', struct.pack('<I', val & 0xFFFFFFFF))[0]
                samples.append(val)
            max_val = 8388608.0  # 2^23
        elif sample_width == 4:  # 32-bit
            samples = struct.unpack(f'<{n_frames * channels}i', raw_data)
            max_val = 2147483648.0
        else:
            raise ValueError(f"Unsupported bit depth: {sample_width * 8}")

        # If stereo, extract only left channel
        if channels == 2:
            samples = samples[::2]
        elif channels > 2:
            raise ValueError(f"Only mono and stereo WAV files supported (got {channels} channels)")

        # Normalize to float [-1.0, 1.0]
        normalized = [s / max_val for s in samples]

        return framerate, normalized


def trim_or_pad_ir(samples, target_length=MAX_IR_SAMPLES):
    """
    Trim or pad IR to target length.

    Args:
        samples: List of float samples
        target_length: Desired sample count

    Returns:
        list: Trimmed/padded samples
    """
    current_length = len(samples)

    if current_length > target_length:
        # Trim from the end
        print(f"  Trimming from {current_length} to {target_length} samples")
        return samples[:target_length]
    elif current_length < target_length:
        # Pad with zeros
        print(f"  Padding from {current_length} to {target_length} samples")
        return samples + [0.0] * (target_length - current_length)
    else:
        return samples


def _fft_recursive(x):
    """Radix-2 Cooley-Tukey FFT. Input length must be a power of 2."""
    N = len(x)
    if N <= 1:
        return x
    even = _fft_recursive(x[0::2])
    odd = _fft_recursive(x[1::2])
    T = [cmath.exp(-2j * cmath.pi * k / N) * odd[k] for k in range(N // 2)]
    return [even[k] + T[k] for k in range(N // 2)] + \
           [even[k] - T[k] for k in range(N // 2)]


def _rfft(samples):
    """Real FFT via Cooley-Tukey. Zero-pads to next power of 2."""
    N = len(samples)
    # Pad to next power of 2
    fft_len = 1
    while fft_len < N:
        fft_len <<= 1
    padded = list(samples) + [0.0] * (fft_len - N)
    full = _fft_recursive(padded)
    # Return positive frequencies only (0 to N/2 inclusive)
    return full[:fft_len // 2 + 1]


def normalize_ir(samples):
    """
    Normalize IR so its peak frequency response magnitude is 0 dB.

    This ensures the IR never amplifies any frequency — it only attenuates
    or passes through. Prevents output clipping when convolving with
    unity-level input signals.

    Args:
        samples: List of float samples

    Returns:
        list: Normalized samples
    """
    spectrum = _rfft(samples)
    peak_mag = max(abs(c) for c in spectrum)

    if peak_mag > 0:
        gain = 1.0 / peak_mag
        peak_db = 20.0 * math.log10(peak_mag)
        print(f"  Freq-response normalization: peak was {peak_db:+.1f} dB, "
              f"scaled by {gain:.4f} ({20.0 * math.log10(gain):+.1f} dB)")
        return [s * gain for s in samples]

    return samples


def format_cpp_raw_array(samples, name, indent=0):
    """
    Format samples as C raw array.

    Args:
        samples: List of float samples
        name: Variable name
        indent: Indentation level

    Returns:
        str: Formatted C++ code
    """
    indent_str = ' ' * indent
    lines = [
        f"{indent_str}__attribute__((aligned(4)))",
        f"{indent_str}const float {name}[{len(samples)}] = {{"
    ]

    # Format samples with 8 values per line
    values_per_line = 8
    for i in range(0, len(samples), values_per_line):
        chunk = samples[i:i + values_per_line]
        formatted_values = ', '.join(f'{v:.8f}f' for v in chunk)
        lines.append(f"{indent_str}    {formatted_values},")

    # Remove trailing comma from last line
    lines[-1] = lines[-1].rstrip(',')

    lines.append(f"{indent_str}}};")

    return '\n'.join(lines)


def sanitize_name(filepath):
    """Convert filepath to valid C++ identifier."""
    name = Path(filepath).stem  # Get filename without extension
    # Replace non-alphanumeric chars with underscore
    name = ''.join(c if c.isalnum() else '_' for c in name)
    # Ensure it doesn't start with a digit
    if name[0].isdigit():
        name = 'ir_' + name
    return name.lower()


def generate_header(ir_data, output_path):
    """
    Generate C++ header file with IR data embedded in firmware.

    Args:
        ir_data: List of (name, samples) tuples
        output_path: Output header file path
    """
    guard_name = "IR_DATA_H"

    lines = [
        "// Auto-generated IR data header",
        "// Do not edit manually - regenerate using wav_to_ir_header.py",
        "//",
        "// IR data is placed in .rodata (copied to SRAM by bootloader at boot).",
        "",
        f"#ifndef {guard_name}",
        f"#define {guard_name}",
        "",
        "#include <cstddef>",
        "",
        "namespace ImpulseResponseData {",
        "",
    ]

    # Add IR metadata structure
    lines.append("// IR metadata")
    lines.append("struct IRInfo {")
    lines.append("    const char* name;")
    lines.append("    const float* data;  // Pointer to IR sample data")
    lines.append("    size_t length;      // Sample count")
    lines.append("};")
    lines.append("")

    # Add individual IR arrays with QSPI attribute
    ir_names = []
    ir_lengths = []
    for name, samples in ir_data:
        lines.append(f"// IR: {name} ({len(samples)} samples, {len(samples)/SAMPLE_RATE*1000:.1f}ms)")
        lines.append(format_cpp_raw_array(samples, name, indent=0))
        lines.append("")
        ir_names.append(name)
        ir_lengths.append(len(samples))

    # Add collection array (array of IRInfo structs)
    lines.append(f"// Collection of all IRs for indexing ({len(ir_names)} total)")
    lines.append(f"constexpr size_t IR_COUNT = {len(ir_names)};")
    lines.append("")
    lines.append("const IRInfo ir_collection[IR_COUNT] = {")
    for name, length in zip(ir_names, ir_lengths):
        lines.append(f'    {{"{name}", {name}, {length}}},')
    lines.append("};")
    lines.append("")

    lines.extend([
        "}  // namespace ImpulseResponseData",
        "",
        f"#endif  // {guard_name}",
        ""
    ])

    # Write to file
    with open(output_path, 'w') as f:
        f.write('\n'.join(lines))

    total_samples = sum(len(samples) for _, samples in ir_data)
    print(f"\nGenerated header: {output_path}")
    print(f"  Total IRs: {len(ir_data)}")
    print(f"  Total samples: {total_samples}")
    print(f"  Estimated flash size: ~{total_samples * 4 / 1024:.1f} KB")
    print(f"  Estimated RAM per IR: ~{max(ir_lengths) * 4 / 1024:.1f} KB")


def main():
    parser = argparse.ArgumentParser(
        description='Convert WAV files to C++ IR header for Flick/MuleBox',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert single IR
  python3 wav_to_ir_header.py irs/v30.wav -o src/ir_data.h

  # Convert multiple IRs (sorted alphabetically for rotary switch positions)
  python3 wav_to_ir_header.py irs/*.wav -o src/ir_data.h
        """
    )

    parser.add_argument('wav_files', nargs='+', help='Input WAV file(s)')
    parser.add_argument('-o', '--output', required=True, help='Output header file path')
    parser.add_argument('--max-length', type=int, default=MAX_IR_LENGTH_MS,
                        help=f'Maximum IR length in ms (default: {MAX_IR_LENGTH_MS})')

    args = parser.parse_args()

    # Validate input files
    wav_files = []
    for pattern in args.wav_files:
        path = Path(pattern)
        if path.exists() and path.is_file():
            wav_files.append(path)
        else:
            print(f"Warning: File not found: {pattern}", file=sys.stderr)

    if not wav_files:
        print("Error: No valid WAV files found", file=sys.stderr)
        return 1

    # Sort alphabetically by filename (case-insensitive) for deterministic
    # rotary switch position mapping: position 1 = first alphabetically, etc.
    wav_files.sort(key=lambda p: p.name.lower())

    if len(wav_files) > MAX_IR_COUNT:
        print(f"Warning: Found {len(wav_files)} files, but only {MAX_IR_COUNT} IRs supported. Using first {MAX_IR_COUNT}.")
        wav_files = wav_files[:MAX_IR_COUNT]

    print(f"\nIR order (sorted alphabetically, maps to rotary positions 1-{len(wav_files)}):")
    for i, wf in enumerate(wav_files):
        print(f"  Position {i+1}: {wf.name}")

    # Process each WAV file
    ir_data = []
    max_samples = int((args.max_length / 1000.0) * SAMPLE_RATE)

    for wav_path in wav_files:
        print(f"\nProcessing: {wav_path}")
        try:
            sample_rate, samples = read_wav_file(wav_path)

            # Validate sample rate
            if sample_rate != SAMPLE_RATE:
                print(f"  Warning: Sample rate is {sample_rate}Hz, expected {SAMPLE_RATE}Hz")
                print(f"  Resampling not implemented - IR may sound incorrect!")

            # Trim or pad to target length
            samples = trim_or_pad_ir(samples, max_samples)

            # Normalize so peak frequency response = 0 dB
            samples = normalize_ir(samples)

            # Generate C++ variable name
            var_name = sanitize_name(wav_path)
            print(f"  Variable name: {var_name}")

            ir_data.append((var_name, samples))

        except Exception as e:
            print(f"  Error processing {wav_path}: {e}", file=sys.stderr)
            continue

    if not ir_data:
        print("\nError: No IRs successfully processed", file=sys.stderr)
        return 1

    # Generate header file
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    generate_header(ir_data, output_path)

    print("\nDone!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
