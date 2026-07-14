"""
config.py

Shared configuration for the THD measurement toolkit's host-side Python
utilities (twiddle-factor generation, Hanning window generation, and ADC
waveform capture/plotting).

NOTE ON A BUG FOUND DURING MERGE:
    The original generate_twiddle_table.py used FFT_LENGTH = 1024, but the
    target file is rfft_q15_128.c and the rest of the project (hanning
    window, ADC capture) all operate on 128-point frames. Generating a
    1024-point twiddle table for a 128-point FFT would silently produce a
    table with the wrong number/values of stages. FFT_LENGTH is fixed to
    128 here. If you actually intended a 1024-point FFT elsewhere, override
    it when calling the twiddle functions instead of changing this shared
    default.
"""

from __future__ import annotations

import math

# Number of samples in one FFT frame / ADC capture / analysis window.
FFT_LENGTH = 128

# Q15 fixed-point scale factor (2**15).
Q15_SCALE = 32768

# Number of points expected in one ADC_AverageValue capture from the device.
ADC_POINTS = 128

# Serial defaults for talking to the STM32F103 board.
DEFAULT_BAUD = 115200
DEFAULT_SERIAL_TIMEOUT_S = 3.0

# Radix-2 FFT stage lengths, e.g. [2, 4, 8, ..., FFT_LENGTH].
STAGE_LENGTHS = [2**k for k in range(1, int(math.log2(FFT_LENGTH)) + 1)]
