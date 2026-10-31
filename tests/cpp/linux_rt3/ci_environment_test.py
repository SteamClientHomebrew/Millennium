import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import textwrap

workflow = pathlib.Path(sys.argv[1]).read_text()
steps = re.findall(r"(?m)^      - name: ([^\n]+)\n        run: \|\n((?:          .*\n|\n)+)", workflow)
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    binary = root / "bin"
    binary.mkdir()
    log = root / "configures.jsonl"
    recorder = binary / "cmake"
    recorder.write_text(f"#!{sys.executable}\n" + "import json,os,sys\nif any(arg.startswith('--preset') for arg in sys.argv):\n with open(os.environ['MILLENNIUM_CI_TEST_LOG'],'a') as f: f.write(json.dumps({'args':sys.argv[1:], 'search':{key:os.environ.get(key,'') for key in ['PKG_CONFIG_PATH','PKG_CONFIG_LIBDIR','CMAKE_LIBRARY_PATH','CMAKE_INCLUDE_PATH','CMAKE_PREFIX_PATH']}})+'\\n')\n")
    recorder.chmod(0o755)
    for name in ["mkdir", "cp"]:
        tool = binary / name
        tool.write_text("#!/bin/sh\nexit 0\n")
        tool.chmod(0o755)
    env = os.environ.copy()
    env.pop("BASH_ENV", None)
    env["PATH"] = str(binary) + os.pathsep + env.get("PATH", "")
    env["HOME"] = str(root)
    env["MILLENNIUM_CI_TEST_LOG"] = str(log)
    for name, script in steps:
        if name in ["Build Millennium Backend", "Build Millennium RT3 Backend"]:
            subprocess.run(["bash", "-eu", "-c", textwrap.dedent(script)], env=env, check=True)
    observed = [json.loads(line) for line in log.read_text().splitlines()]
    assert any("--preset=linux-release" in entry["args"] for entry in observed), observed
    native = [entry for entry in observed if "--preset=linux-release-x64" in entry["args"]]
    assert len(native) == 1, observed
    assert all("i386" not in value for value in native[0]["search"].values()), native[0]
