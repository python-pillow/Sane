#!/usr/bin/env python3
"""Run a snap() scan against the LD_PRELOADed mock libsane and verify the
returned image bytes against the deterministic stream the mock serves.

Invoked as a subprocess by tests/test_snap_params_cache.py with LD_PRELOAD
set to the compiled mock shim. Exits 0 on success, non-zero otherwise.

Scenario knobs must match those passed to the mock (see libsane_mock.c).
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, os.environ.get("SNAP_MOCK_REPO_ROOT",
                                  str(Path(__file__).resolve().parents[2])))

import sane  # noqa: E402  (needs the repo root on sys.path first)


def mock_pattern(k):
    return (k * 37 + 11) % 256


def main():
    depth = int(os.environ.get("SNAP_MOCK_DEPTH", "8"))
    partial = int(os.environ.get("SNAP_MOCK_PARTIAL", "0"))
    skip_get_params = os.environ.get("SNAP_MOCK_SKIP_GET_PARAMS", "0") == "1"

    if depth == 1:
        ppl, bpl, lines = 1700, 213, 2280
    elif depth == 8:
        ppl, bpl, lines = 2280, 2280, 2280
    else:
        print("unsupported SNAP_MOCK_DEPTH", depth)
        return 2

    sane.init()
    dev = sane.open("mock:0")
    dev.start()

    if not skip_get_params:
        params = dev.get_parameters()
        fmt, last_frame, (ppl2, lines2), depth2, bpl2 = params
        assert (fmt, last_frame, ppl2, lines2, depth2, bpl2) == \
            ("gray", 1, ppl, lines, depth, bpl), params

    data, width, height, samples, sample_size = dev.dev.snap()

    stream_len = lines * bpl - (7 if partial else 0)
    stream = bytes(mock_pattern(k) for k in range(stream_len))

    if depth == 1:
        expected = bytearray(lines * ppl)
        for line in range(lines):
            base = line * ppl
            sbase = line * bpl
            for x in range(ppl):
                b = stream[sbase + x // 8]
                expected[base + x] = 0 if ((b >> (7 - (x % 8))) & 1) else 255
        expected = bytes(expected)
    else:
        expected = stream + (bytes(7) if partial else b"")

    assert (width, height, samples, sample_size) == (ppl, lines, 1, 1), \
        (width, height, samples, sample_size)
    assert len(data) == len(expected), (len(data), len(expected))
    assert bytes(data) == expected, "image bytes mismatch"

    print("OK depth=%d partial=%d skip_get_params=%d" % (depth, partial,
          skip_get_params))
    return 0


if __name__ == "__main__":
    sys.exit(main())
