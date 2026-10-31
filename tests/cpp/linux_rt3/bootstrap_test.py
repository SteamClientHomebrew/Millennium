import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

proxy = str(pathlib.Path(sys.argv[1]).resolve())
client = pathlib.Path(sys.argv[2]).resolve()
mode = sys.argv[3]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    (root / "steamrt64").mkdir()
    executable = root / "steamrt64" / ("steamwebhelper" if mode == "webhelper" else "steam" if mode != "forwarding" else "other")
    shutil.copy2(client, executable)
    env = os.environ.copy()
    env["LD_PRELOAD"] = proxy
    command = [str(executable)]
    if mode == "updater-ui":
        command.append("-child-update-ui")
    elif mode == "updater-socket":
        command.append("-child-update-ui-socket")
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
    expected_starts = 1 if mode == "client" else 0
    assert result.stdout.count("MILLENNIUM_TEST_STARTED") == expected_starts, result.stdout
    assert result.stdout.count("MILLENNIUM_TEST_STOPPED") == expected_starts, result.stdout
    if mode == "webhelper":
        assert "Loaded hhx64 from" in result.stdout, result.stdout + result.stderr
