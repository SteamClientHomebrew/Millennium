import json
import os
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
mode = sys.argv[2]
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    holders = []
    try:
        for name in ["cmd.fifo", "resp.fifo", "lb-cmd.fifo", "lb-resp.fifo"]:
            path = root / name
            os.mkfifo(path, 0o600)
            holders.append(os.open(path, os.O_RDWR | os.O_CLOEXEC))
        env = os.environ.copy()
        env["PRESSURE_VESSEL_PREFIX"] = str(root)
        env["MILLENNIUM_PVS64_DIRECT_EXEC"] = "1"
        (root / "bin").mkdir()
        target = root / "bin" / "pvs-target"
        target.symlink_to(sys.executable)
        env["PATH"] = str(root / "bin") + os.pathsep + env.get("PATH", "")
        child = "import fcntl,json,os,stat,sys; print(json.dumps({'fds': [[stat.S_ISFIFO(os.fstat(fd).st_mode), fcntl.fcntl(fd,fcntl.F_GETFL) & os.O_ACCMODE, os.get_inheritable(fd)] for fd in range(3,7)], 'args': sys.argv[1:], 'clean': not any(key in os.environ for key in ['PRESSURE_VESSEL_PREFIX','MILLENNIUM_PVS64_DIRECT_EXEC'])}))"
        command = [binary]
        if mode != "no-command":
            command.extend(["pvs-target" if mode == "path" else str(target), "-c", child, "argument with spaces"])
        result = subprocess.run(command, env=env, cwd=root, capture_output=True, text=True, timeout=10)
        if mode == "no-command":
            assert result.returncode == 1, result
            assert "without a command" in result.stderr, result.stderr
        else:
            assert result.returncode == 0, result.stderr
            actual = json.loads(result.stdout)
            assert actual["fds"] == [[True, os.O_RDONLY, True], [True, os.O_WRONLY, True], [True, os.O_RDONLY, True], [True, os.O_WRONLY, True]], actual
            assert actual["args"] == ["argument with spaces"], actual
            assert actual["clean"], actual
        assert not any((root / name).exists() for name in ["cmd.fifo", "resp.fifo", "lb-cmd.fifo", "lb-resp.fifo"])
    finally:
        for fd in holders:
            os.close(fd)
