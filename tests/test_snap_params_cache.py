"""Regression test for the SaneDev_snap params-cache fix (finding 18).

The epsonscan2 backend returns INVAL on a second sane_get_parameters() call
after sane_start. The wrapper's get_parameters() succeeds; snap()'s redundant
call inside the C extension fails. The fix caches params from
SaneDev_get_parameters and reuses them in SaneDev_snap.

Runs a real snap() through the C extension against an LD_PRELOADed mock
libsane that replicates this behavior. The mock is loaded via LD_PRELOAD in
a dedicated subprocess so it only shadows libsane inside the test's
controlled process.
"""
import os
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MOCK_SRC = ROOT / "tests" / "mock" / "libsane_mock.c"
RUNNER = ROOT / "tests" / "mock" / "run_snap_mock.py"
SHIM = ROOT / "build" / "libsane_mock.so"


@pytest.fixture(scope="module", autouse=True)
def shim():
    (ROOT / "build").mkdir(exist_ok=True)
    subprocess.run(
        ["gcc", "-shared", "-fPIC", "-O2", "-o", str(SHIM), str(MOCK_SRC)],
        check=True,
    )
    if not list(ROOT.glob("_sane.cpython-*.so")):
        subprocess.run(
            [sys.executable, "setup.py", "build_ext", "--inplace"],
            cwd=ROOT,
            check=True,
        )
    return SHIM


def run_scenario(env_extra):
    env = os.environ.copy()
    env["LD_PRELOAD"] = str(SHIM)
    env.update(env_extra)
    proc = subprocess.run(
        [sys.executable, str(RUNNER)],
        env=env,
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=120,
    )
    assert proc.returncode == 0, "scenario failed:\n%s\n%s" % (
        proc.stdout,
        proc.stderr,
    )


def test_depth8():
    run_scenario({"SNAP_MOCK_DEPTH": "8"})


def test_depth1():
    run_scenario({"SNAP_MOCK_DEPTH": "1"})


def test_snap_without_prior_get_parameters():
    """snap() without a prior get_parameters() should still succeed via the
    fallback path in SaneDev_snap (finding 18 fix: cached params avoid a
    second sane_get_parameters call, but when the cache is empty snap()
    calls sane_get_parameters once as a fallback)."""
    run_scenario({"SNAP_MOCK_DEPTH": "8", "SNAP_MOCK_SKIP_GET_PARAMS": "1"})
