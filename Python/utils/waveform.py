"""
waveform.py

Reads the 128-point ADC_AverageValue array from the STM32F103 THD device and
plots it as a waveform image. Supports two modes:

1. Serial mode (default): connect to the device over a serial port, send the
   'S' command, and parse the response live.

2. Offline mode: no serial connection needed. Paste terminal output (or pipe
   a saved log/file) containing the "ADC_AverageValue[128]:" header followed
   by the comma-separated data line, and this module will extract and plot
   it. Useful after the serial port/monitor has already been closed but you
   still have the printed data sitting in your terminal scrollback or a
   saved log file.

Protocol expected from the device (triggered by sending the 'S' command byte):

    ADC_AverageValue[128]:
    1886,1884,1881,...,1892

Refactored from the original read_adc_waveform.py:
  - No longer has its own __main__/argparse block; main.py's `waveform`
    subcommand drives this module so all three tools share one CLI.
  - EXPECTED_POINTS / default baud / default timeout now come from
    config.py instead of being redefined here.
"""

from __future__ import annotations

import sys
import time
from typing import Iterable, Optional

import matplotlib

matplotlib.use("Agg")  # non-interactive backend, safe for headless/script use
import matplotlib.pyplot as plt

from utils.config import ADC_POINTS, DEFAULT_BAUD, DEFAULT_SERIAL_TIMEOUT_S

HEADER_PREFIX = "ADC_AverageValue["


def parse_frame_from_lines(lines: Iterable[str]) -> list[int]:
    """
    Scan an iterable of text lines for the ADC_AverageValue header followed by
    its comma-separated data line, and return the parsed list of ints.

    Shared by both serial mode (lines come from the live port) and offline
    mode (lines come from pasted stdin or a saved file).

    Raises RuntimeError if the header/data was not found or malformed.
    """
    header_found = False

    for raw_line in lines:
        line = raw_line.strip()

        if not header_found:
            if line.startswith(HEADER_PREFIX):
                header_found = True
            continue

        # header_found is True: this line should be the comma-separated data
        if not line:
            continue

        try:
            values = [int(v) for v in line.split(",") if v != ""]
        except ValueError as exc:
            raise RuntimeError(f"Failed to parse data line: {line!r}") from exc

        if len(values) != ADC_POINTS:
            raise RuntimeError(
                f"Expected {ADC_POINTS} points, got {len(values)}. Line was: {line!r}"
            )

        return values

    if header_found:
        raise RuntimeError(
            f"Found the '{HEADER_PREFIX}...' header but no data line followed it."
        )
    raise RuntimeError(
        f"Could not find a line starting with '{HEADER_PREFIX}' in the input."
    )


def read_one_frame_serial(
    ser, timeout_s: float = DEFAULT_SERIAL_TIMEOUT_S
) -> list[int]:
    """
    Send the 'S' command over an open serial connection and read back one
    frame of ADC_AverageValue samples.
    """
    # Clear any stale bytes sitting in the input buffer before requesting fresh data
    ser.reset_input_buffer()
    ser.write(b"S")

    deadline = time.monotonic() + timeout_s

    def line_generator():
        while time.monotonic() < deadline:
            raw_line = ser.readline()
            if not raw_line:
                continue  # readline timed out on this attempt, keep trying until deadline
            yield raw_line.decode("ascii", errors="ignore")

    try:
        return parse_frame_from_lines(line_generator())
    except RuntimeError:
        raise RuntimeError(
            "Timed out waiting for ADC_AverageValue data from the device. "
            "Check the port, baud rate, and that the device is running."
        )


def read_one_frame_offline(input_file: Optional[str]) -> list[int]:
    """
    Parse one frame from either a file (--input-file) or from stdin
    (interactive paste, or piped input).
    """
    if input_file:
        with open(input_file, "r", encoding="ascii", errors="ignore") as f:
            return parse_frame_from_lines(f)

    if sys.stdin.isatty():
        print(
            "Paste the terminal output containing 'ADC_AverageValue[128]:' "
            "and its data line below.\n"
            "When done, press Ctrl+Z then Enter (Windows) or Ctrl+D (Linux/Mac):",
            file=sys.stderr,
        )

    return parse_frame_from_lines(sys.stdin)


def plot_waveform(
    values: list[int], output_path: str, title: str = "ADC Waveform"
) -> None:
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(range(len(values)), values, marker=".", linewidth=1)
    ax.set_title(title)
    ax.set_xlabel(f"Sample index (0-{len(values) - 1})")
    ax.set_ylabel("ADC value (12-bit averaged)")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    plt.close(fig)


def run(args) -> int:
    """Entry point used by main.py's `waveform` subcommand.

    `args` is the argparse.Namespace produced by main.py's waveform
    subparser (see main.py for the exact option names).
    """
    if args.offline:
        try:
            values = read_one_frame_offline(args.input_file)
        except RuntimeError as exc:
            print(f"Error: {exc}", file=sys.stderr)
            return 1

        plot_waveform(values, args.output)
        print(f"Saved waveform ({len(values)} points) to {args.output}")
        return 0

    # --- Serial mode from here on ---
    if not args.port:
        print("Error: --port is required unless --offline is used.", file=sys.stderr)
        return 1

    try:
        import serial
    except ImportError:
        print(
            "Error: pyserial is not installed. Run 'pip install pyserial', "
            "or use --offline mode if you don't need a live connection.",
            file=sys.stderr,
        )
        return 1

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.5)
    except serial.SerialException as exc:
        print(f"Failed to open serial port {args.port}: {exc}", file=sys.stderr)
        return 1

    try:
        if not args.continuous:
            values = read_one_frame_serial(ser, timeout_s=args.timeout)
            plot_waveform(values, args.output)
            print(f"Saved waveform ({len(values)} points) to {args.output}")
            return 0

        print("Continuous mode: press Ctrl+C to stop.")
        frame_count = 0
        try:
            while True:
                try:
                    values = read_one_frame_serial(ser, timeout_s=args.timeout)
                    plot_waveform(
                        values,
                        args.output,
                        title=f"ADC Waveform (frame {frame_count})",
                    )
                    frame_count += 1
                    print(f"[frame {frame_count}] saved to {args.output}")
                except RuntimeError as exc:
                    print(f"Warning: {exc}", file=sys.stderr)

                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\nStopped by user.")
            return 0

    finally:
        ser.close()
