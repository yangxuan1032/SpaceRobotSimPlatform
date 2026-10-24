"""SRSP Python sidecar: serves a Python plugin over the gRPC PluginRuntime API.

Usage: srsp_sidecar.py --port <n> --token <t> [--plugin <path>]

The script prints "READY <port>" once the server is bound, so the C++ host can
drive it exactly like a native sidecar. `--plugin` is accepted for interface
symmetry; a Python sidecar implements its plugin in this file.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import threading
from concurrent import futures

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "generated"))

import grpc  # noqa: E402

from srsp.v1 import plugin_pb2, plugin_pb2_grpc  # noqa: E402

#: Host ABI major version this sidecar accepts (keep in sync with plugin_api.h).
HOST_ABI_MAJOR = 1


class PythonPlugin:
    """Minimal Python-side plugin used to prove the cross-language path."""

    def __init__(self) -> None:
        self.steps = 0

    def invoke(self, method: str) -> bytes | None:
        if method == "core.describe":
            return json.dumps(
                {
                    "id": "org.srsp.python.hello",
                    "version": "0.1.0",
                    "lang": "python",
                    "methods": ["core.describe", "core.ping", "controller.update"],
                }
            ).encode("utf-8")
        if method == "core.ping":
            return json.dumps(
                {"ok": True, "steps": self.steps, "lang": "python"}
            ).encode("utf-8")
        if method == "controller.update":
            self.steps += 1
            return json.dumps(
                {"commands": [], "steps": self.steps, "lang": "python"}
            ).encode("utf-8")
        return None


class Runtime(plugin_pb2_grpc.PluginRuntimeServicer):
    def __init__(self, token: str, fault: str = "") -> None:
        self._token = token
        self._plugin = PythonPlugin()
        self._stop = threading.Event()
        # Test hook: injected once, used by the CLI restart test.
        self._fault = fault
        self._fault_fired = False

    def Handshake(self, request, context):  # noqa: N802
        if request.token != self._token:
            context.abort(grpc.StatusCode.PERMISSION_DENIED, "bad token")
        if request.host_abi_major != HOST_ABI_MAJOR:
            context.abort(grpc.StatusCode.FAILED_PRECONDITION, "unsupported host ABI")
        return plugin_pb2.HandshakeReply(
            plugin_id="org.srsp.python.hello", api="1.0", transport="grpc"
        )

    def AttachShm(self, request, context):  # noqa: N802
        context.abort(grpc.StatusCode.UNIMPLEMENTED, "shared memory is reserved for M2")

    def Invoke(self, request, context):  # noqa: N802
        if self._fault == "first-invoke" and not self._fault_fired:
            self._fault_fired = True
            return plugin_pb2.InvokeResponse(
                error=plugin_pb2.Error(code=11, message="injected fault")
            )
        payload = self._plugin.invoke(request.method)
        if payload is None:
            return plugin_pb2.InvokeResponse(
                error=plugin_pb2.Error(code=2, message="unknown method")
            )
        return plugin_pb2.InvokeResponse(payload=payload)

    def Ping(self, request, context):  # noqa: N802
        return plugin_pb2.PingReply(ok=True, t_tai=0.0, step_index=request.step_index)

    def Checkpoint(self, request, context):  # noqa: N802
        return plugin_pb2.BlobReply(
            inline_data=json.dumps({"steps": self._plugin.steps}).encode("utf-8")
        )

    def Restore(self, request, context):  # noqa: N802
        try:
            data = json.loads(request.inline_data.decode("utf-8") or "{}")
        except json.JSONDecodeError:
            context.abort(grpc.StatusCode.FAILED_PRECONDITION, "bad snapshot")
        self._plugin.steps = int(data.get("steps", 0))
        return plugin_pb2.Empty()

    def Shutdown(self, request, context):  # noqa: N802
        self._stop.set()
        return plugin_pb2.Empty()


def main() -> int:
    parser = argparse.ArgumentParser(description="SRSP Python sidecar")
    parser.add_argument("--port", type=int, default=0, help="bind port (0 = ephemeral)")
    parser.add_argument("--token", required=True, help="handshake token")
    parser.add_argument("--plugin", default="", help="accepted for interface symmetry")
    args = parser.parse_args()

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))
    runtime = Runtime(args.token, os.environ.get("SRSP_SIDECAR_FAULT", ""))
    plugin_pb2_grpc.add_PluginRuntimeServicer_to_server(runtime, server)
    port = server.add_insecure_port(f"127.0.0.1:{args.port}")
    if port == 0:
        print("error: could not bind a port", file=sys.stderr)
        return 1
    server.start()
    print(f"READY {port}", flush=True)

    runtime._stop.wait()
    server.stop(grace=1.0).wait(timeout=2.0)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
