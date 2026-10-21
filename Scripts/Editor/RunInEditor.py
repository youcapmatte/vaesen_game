"""Runs a Python script inside an already open Unreal Editor (Python Remote Execution), and prints its output.

    python -I Scripts/Editor/RunInEditor.py <script.py> [args...]

The editor must have Remote Execution on (Project Settings > Plugins > Python), bound to 127.0.0.1. Nothing is started:
if no editor answers within a few seconds, this exits with an error. Exit code 0 when the script ran without an
exception, 1 otherwise.
"""
import os
import sys
import time

UE_ROOT = os.environ.get("UE_ROOT", r"F:\UE_SOURCE\UE5_8")
sys.path.insert(0, os.path.join(UE_ROOT, r"Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python"))
import remote_execution  # noqa: E402  (the engine's own client)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    script = os.path.abspath(sys.argv[1]).replace("\\", "/")
    command = " ".join([f'"{script}"'] + [f'"{a}"' for a in sys.argv[2:]])

    remote = remote_execution.RemoteExecution()
    remote.start()
    try:
        deadline = time.time() + 10
        while not remote.remote_nodes and time.time() < deadline:
            time.sleep(0.25)
        nodes = remote.remote_nodes
        if not nodes:
            print("No open Unreal Editor with Remote Execution found (is it running, with Remote Execution on?).")
            return 1
        remote.open_command_connection(nodes[0]["node_id"])
        result = remote.run_command(command, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
        for entry in result.get("output", []):
            print(entry.get("output", "").rstrip())
        if not result.get("success"):
            print(result.get("result", ""))
            return 1
        return 0
    finally:
        remote.stop()


if __name__ == "__main__":
    sys.exit(main())
