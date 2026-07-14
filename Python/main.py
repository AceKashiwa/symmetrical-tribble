#!/usr/bin/env python3
"""
main.py

Unified command-line entry point for the STM32F103 THD project's host-side
Python tools. Merges what used to be three separate scripts into one CLI
with subcommands, all sharing the config in config.py:

  twiddle   - generate_twiddle_table.py's functionality
  waveform  - read_adc_waveform.py's functionality

Examples:
    python main.py twiddle
    python main.py twiddle --update path/to/rfft_q15_128.c
    python main.py waveform --port COM5
    python main.py waveform --offline --input-file capture.txt --output waveform.png
"""

from __future__ import annotations

import argparse
import sys

import utils.twiddle as twiddle
import utils.waveform as waveform
from utils.config import DEFAULT_BAUD, DEFAULT_SERIAL_TIMEOUT_S, FFT_LENGTH


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="THD project host-side toolkit: twiddle table"
        "window, and ADC waveform capture/plotting.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # --- twiddle ---
    p_twiddle = subparsers.add_parser(
        "twiddle",
        help="Generate/update the FFT twiddle factor table for rfft_q15_128.c",
    )
    p_twiddle.add_argument(
        "--update",
        metavar="SOURCE",
        default=None,
        help="Update the twiddle table in the given C source file in-place "
        "instead of printing to stdout.",
    )

    # --- waveform ---
    p_waveform = subparsers.add_parser(
        "waveform",
        help="Capture (serial or offline) and plot the ADC_AverageValue waveform",
    )
    p_waveform.add_argument(
        "--offline",
        action="store_true",
        help="Skip the serial connection entirely; parse data from --input-file "
        "or from stdin (paste or pipe) instead.",
    )
    p_waveform.add_argument(
        "--input-file",
        default=None,
        help="(Offline mode) Read data from this file instead of stdin.",
    )
    p_waveform.add_argument(
        "--port",
        default=None,
        help="Serial port, e.g. COM5 or /dev/ttyUSB0 (required unless --offline)",
    )
    p_waveform.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=f"Baud rate (default: {DEFAULT_BAUD})",
    )
    p_waveform.add_argument(
        "--output",
        default="waveform.png",
        help="Output image path (default: waveform.png). In --continuous mode "
        "this file is overwritten each cycle.",
    )
    p_waveform.add_argument(
        "--continuous",
        action="store_true",
        help="(Serial mode only) Keep polling and re-plotting at --interval "
        "seconds until Ctrl+C.",
    )
    p_waveform.add_argument(
        "--interval",
        type=float,
        default=1.0,
        help="Seconds between polls when --continuous is set (default: 1.0)",
    )
    p_waveform.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_SERIAL_TIMEOUT_S,
        help=f"Seconds to wait for a frame before giving up (default: {DEFAULT_SERIAL_TIMEOUT_S})",
    )

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "twiddle":
        twiddle.run(args.update)
        return 0

    if args.command == "waveform":
        return waveform.run(args)

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
