"""
twiddle.py

Generate precomputed FFT twiddle factors for rfft_q15_128.c.

Refactored from the original generate_twiddle_table.py:
  - FFT_LENGTH now comes from config.py (fixed to 128 to match the actual
    128-point FFT used elsewhere in the project; see config.py docstring).
  - Table building and in-place file update are split into small,
    independently testable/importable functions instead of only being
    reachable via a standalone script.
"""

from __future__ import annotations

import math
import re
from typing import List, Tuple

from utils.config import STAGE_LENGTHS, Q15_SCALE

TABLE_HEADER = "static const Complex twiddle_factors[] = {"
TABLE_FOOTER = "};"

_TABLE_PATTERN = re.compile(
    r"static const Complex twiddle_factors\[\] = \{.*?\};\n",
    re.DOTALL,
)


def compute_twiddle_factors(
    lengths: List[int] = STAGE_LENGTHS,
) -> List[Tuple[int, int]]:
    """Compute (re, im) Q15 twiddle factor pairs for each stage length."""
    return [
        (
            int(math.cos(-2.0 * math.pi / length) * Q15_SCALE),
            int(math.sin(-2.0 * math.pi / length) * Q15_SCALE),
        )
        for length in lengths
    ]


def format_table(
    factors: List[Tuple[int, int]], lengths: List[int] = STAGE_LENGTHS
) -> str:
    """Render twiddle factors as a C initializer list."""
    lines = [TABLE_HEADER]
    for length, (re_val, im_val) in zip(lengths, factors):
        lines.append(f"  /* len={length:<3}*/ {{ {re_val}, {im_val} }},")
    lines.append(TABLE_FOOTER)
    return "\n".join(lines)


def build_source_table() -> str:
    """Build the full C twiddle-factor table text using the default stage lengths."""
    factors = compute_twiddle_factors(STAGE_LENGTHS)
    return format_table(factors, STAGE_LENGTHS)


def update_source_file(path: str) -> None:
    """Replace the existing twiddle_factors table in a C source file in-place."""
    with open(path, "r", encoding="utf-8") as f:
        source = f.read()

    new_table = build_source_table() + "\n\n"
    new_source, count = _TABLE_PATTERN.subn(new_table, source, count=1)
    if count == 0:
        raise ValueError(
            "Could not find existing twiddle_factors table in source file."
        )

    with open(path, "w", encoding="utf-8") as f:
        f.write(new_source)

    print(f"Updated twiddle table in {path}")


def run(update_path: str | None) -> None:
    """Entry point used by main.py's `twiddle` subcommand."""
    if update_path:
        update_source_file(update_path)
    else:
        print(build_source_table())
