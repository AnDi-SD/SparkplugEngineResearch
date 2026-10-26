"""Bounded cross-process tests of WinxCoop's real TCP diagnostic executable.

Every transport operation here uses the OS. The fault peer is independent
Python socket code and literal packet fixtures. A successful run proves this
host's TCP paths; it does not imply two guest OSes or a physical second PC.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
import traceback
from typing import Any, Callable

from transport_fault_peer import (
    INFO, PROBE, READY, abort, connect, drain_until_closed, handshake_guest,
    oversized_header, packet_bytes, receive_packet,
)


ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "tools/WinxCoop"
HOST_REQUEST = 0x57434F50
GUEST_REQUEST = 0x57434F52
STRESS_CLASS = 0x57435354
PHASES = {0: "Idle", 1: "AwaitingProbe", 2: "AwaitingInfo", 3: "AwaitingReady",
          4: "WaitingForHost", 5: "Running", 6: "Closed", 7: "Failed"}
ERRORS = {0: "None", 1: "InvalidPacket", 2: "InvalidIdentity", 3: "UnexpectedPacket",
          4: "InvalidAssignment", 5: "FramingFailure", 6: "TransportFailure",
          7: "Timeout", 8: "ClockMovedBackward", 9: "Backpressure",
          10: "ResourceFailure", 11: "CodecFailure"}


def check(condition: bool, description: str) -> None:
    if not condition:
        raise AssertionError(description)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def source_hashes() -> dict[str, str]:
    selected: set[Path] = set()
    for directory in (TOOL / "src", TOOL / "tests"):
        selected.update(path for path in directory.rglob("*")
                        if path.is_file() and path.suffix in {".h", ".cpp", ".py"}
                        and "__pycache__" not in path.parts)
    selected.update(TOOL / name for name in ("CMakeLists.txt", "Build-NetworkChecks.ps1"))
    selected.update(ROOT / name for name in (
        "Sparkplug/Code/Sparkplug/spNetworkPacket.h", "Sparkplug/Code/Sparkplug/spNetworkPacket.cpp",
        "Sparkplug/Code/SparkBase/spMemoryStream.h", "Sparkplug/Code/SparkBase/spMemoryStream.cpp",
        "Sparkplug/Code/SparkBase/spStream.h", "Sparkplug/Code/SparkBase/spStream.cpp"))
    return {path.relative_to(ROOT).as_posix(): sha(path) for path in sorted(selected)}


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def label(value: Any, mapping: dict[int, str]) -> str:
    return mapping.get(value, str(value)) if isinstance(value, int) else str(value)


def events(report: dict[str, Any], *, phase: str | None = None,
           error: str | None = None) -> list[dict[str, Any]]:
    return [event for event in report.get("events", [])
            if (phase is None or label(event.get("phase"), PHASES) == phase)
            and (error is None or label(event.get("error"), ERRORS) == error)]


def metric(report: dict[str, Any], name: str) -> int:
    value = report.get("metrics", {}).get(name)
    check(isinstance(value, int) and not isinstance(value, bool), f"Missing integer metric {name}")
    return value


def socket_errors(report: dict[str, Any]) -> set[int]:
    result: set[int] = set()
    for container in (report, report.get("metrics", {}), *report.get("events", [])):
        for key in ("lastSocketError", "socketError", "socketErrorCode", "transportErrorCode"):
            value = container.get(key)
            if isinstance(value, int) and value:
                result.add(value)
    return result


def expected_payload(size: int, role_index: int) -> str:
    return bytes((size + index * 17 + role_index * 31) & 255
                 for index in range(size)).hex()


def normalized_messages(report: dict[str, Any], connection_id: int,
                        direction: str) -> list[tuple[int, int, str]]:
    return [(int(message["classId"]), int(message["key"]), message["payloadHex"].lower())
            for message in report.get("messages", [])
            if int(message["connectionId"]) == connection_id and message["direction"] == direction]


def assert_exchange(report: dict[str, Any], *, role: str,
                    connections: int = 1) -> None:
    check(report.get("probeErrors") == [], f"Probe errors: {report.get('probeErrors')}")
    check(report.get("role") == role, "Reported role matches the launched role")
    check(report.get("ownedSocketsAfterClose") == 0, "All owned sockets are closed")
    running = events(report, phase="Running")
    ids = sorted({int(event["connectionId"]) for event in running})
    check(len(ids) == connections, f"Expected {connections} independent Running connections, got {ids}")
    own_class = HOST_REQUEST if role == "host" else GUEST_REQUEST
    other_class = GUEST_REQUEST if role == "host" else HOST_REQUEST
    own_index = 0 if role == "host" else 1
    other_index = 1 - own_index
    expected_sent = sorted(
        [(own_class, size, expected_payload(size, own_index)) for size in (0, 1, 256)]
        + [(other_class + 1, size, expected_payload(size, other_index)) for size in (0, 1, 256)])
    expected_received = sorted(
        [(other_class, size, expected_payload(size, other_index)) for size in (0, 1, 256)]
        + [(own_class + 1, size, expected_payload(size, own_index)) for size in (0, 1, 256)])
    for connection_id in ids:
        check(sorted(normalized_messages(report, connection_id, "sent")) == expected_sent,
              f"Exact sent corpus/replies mismatch on {role} connection {connection_id}")
        check(sorted(normalized_messages(report, connection_id, "received")) == expected_received,
              f"Exact received corpus/replies mismatch on {role} connection {connection_id}")
    check(metric(report, "bytesSent") > 0 and metric(report, "bytesReceived") > 0,
          "Actual OS byte counters are positive")
    check(metric(report, "sendCalls") > 0 and metric(report, "recvCalls") > 0,
          "Actual OS calls were made")


@dataclass
class Probe:
    process: subprocess.Popen[Any]
    command: list[str]
    report_path: Path
    ready_path: Path
    stdout_path: Path
    stderr_path: Path
    stdout_handle: Any
    stderr_handle: Any
    ready: dict[str, Any] | None = None

    def wait_ready(self, seconds: float = 5.0) -> dict[str, Any]:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if self.ready_path.exists():
                try:
                    ready = read_json(self.ready_path)
                    check(ready["pid"] == self.process.pid, "Ready PID belongs to the launched probe")
                    bound = ready["bound"]
                    check(isinstance(bound["address"], str) and 0 < bound["port"] < 65536,
                          "OS selected a real nonzero bound port")
                    self.ready = ready
                    return ready
                except (json.JSONDecodeError, PermissionError):
                    pass
            if self.process.poll() is not None:
                raise AssertionError(f"Probe exited {self.process.returncode} before ready: {self.command}")
            time.sleep(0.02)
        raise TimeoutError(f"Probe ready deadline: {self.command}")

    def finish(self, expected_exit: int = 0, seconds: float = 10.0) -> dict[str, Any]:
        code = self.process.wait(timeout=seconds)
        self.stdout_handle.flush(); self.stderr_handle.flush()
        check(code == expected_exit, f"Probe exit {code}, expected {expected_exit}: {self.command}")
        check(self.report_path.is_file(), "Probe wrote a final report")
        report = read_json(self.report_path)
        check(report["pid"] == self.process.pid, "Report PID belongs to the launched probe")
        if self.ready:
            check(report["bound"] == self.ready["bound"], "Bound endpoint retained in final report")
        check(report.get("ownedSocketsAfterClose") == 0, "Terminal paths close every owned socket")
        check(metric(report,"socketsOpened") == metric(report,"socketsClosed")
              and metric(report,"socketCloseErrors") == 0,
              "Each actual owned socket is closed exactly once without close errors")
        check(metric(report,"wsaStarts") == metric(report,"wsaCleanups")
              and metric(report,"wsaCleanupErrors") == 0,
              "Actual Winsock startup and cleanup calls are balanced")
        check(metric(report,"droppedSessionEvents") == 0, "No lifecycle evidence was dropped")
        return report

    def snapshot(self) -> dict[str, Any]:
        result: dict[str, Any] = {"pid": self.process.pid, "command": self.command,
                                  "exitCode": self.process.poll(), "ready": self.ready,
                                  "stdout": str(self.stdout_path), "stderr": str(self.stderr_path)}
        if self.report_path.exists():
            try:
                result["report"] = read_json(self.report_path)
                result["reportSha256"] = sha(self.report_path)
            except (json.JSONDecodeError, PermissionError) as error:
                result["reportReadError"] = repr(error)
        return result

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.wait(timeout=2)
        self.stdout_handle.close(); self.stderr_handle.close()


class Case:
    def __init__(self, executable: Path, directory: Path, child_seconds: float):
        self.executable = executable
        self.directory = directory
        self.child_seconds = child_seconds
        self.probes: list[Probe] = []
        self.observations: dict[str, Any] = {}

    def launch(self, name: str, role: str, *, address: str = "127.0.0.1",
               server: tuple[str, int] | None = None, run_ms: int = 1800,
               start_delay_ms: int | None = 0, preauthorize: bool = False,
               send_chunk: int = 65536, receive_chunk: int = 65536,
               heartbeat_ms: int = 100, timeout_ms: int = 1500,
               echo_sizes: bool = True, expect_connections: int = 1,
               exit_after_exchange: bool = False, bind_port: int = 0,
               extra_args: list[str] | None = None) -> Probe:
        report_path = self.directory / f"{name}.report.json"
        ready_path = self.directory / f"{name}.ready.json"
        stdout_path = self.directory / f"{name}.stdout.log"
        stderr_path = self.directory / f"{name}.stderr.log"
        for path in (report_path, ready_path, stdout_path, stderr_path):
            check(not path.exists(), f"Fresh probe output required: {path}")
        command = [str(self.executable), "--role", role, "--bind", f"{address}:{bind_port}",
                   "--report", str(report_path), "--ready-file", str(ready_path),
                   "--run-ms", str(run_ms), "--send-chunk", str(send_chunk),
                   "--recv-chunk", str(receive_chunk), "--heartbeat-ms", str(heartbeat_ms),
                   "--timeout-ms", str(timeout_ms), "--expect-connections", str(expect_connections)]
        if role == "host":
            if preauthorize:
                command.append("--preauthorize")
            elif start_delay_ms is not None:
                command += ["--start-delay-ms", str(start_delay_ms)]
        else:
            check(server is not None, "Guest requires a concrete server endpoint")
            command += ["--server", f"{server[0]}:{server[1]}"]
        if echo_sizes:
            command += ["--echo-sizes", "0,1,256"]
        if exit_after_exchange:
            command.append("--exit-after-exchange")
        if extra_args:
            command += extra_args
        out = stdout_path.open("x", encoding="utf-8")
        err = stderr_path.open("x", encoding="utf-8")
        try:
            process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.DEVNULL,
                                       stdout=out, stderr=err,
                                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        except BaseException:
            out.close(); err.close(); raise
        probe = Probe(process, command, report_path, ready_path, stdout_path, stderr_path, out, err)
        self.probes.append(probe)
        return probe

    def endpoint(self, host: Probe) -> tuple[str, int]:
        bound = host.wait_ready()["bound"]
        return bound["address"], int(bound["port"])

    def close(self) -> None:
        for probe in reversed(self.probes):
            probe.close()

    def snapshots(self) -> list[dict[str, Any]]:
        return [probe.snapshot() for probe in self.probes]


def exchange(case: Case, *, address: str = "127.0.0.1", chunk: int = 65536,
             delay: int = 0, preauthorize: bool = False) -> None:
    host = case.launch("host", "host", address=address, run_ms=2200,
                       start_delay_ms=delay, preauthorize=preauthorize,
                       send_chunk=chunk, receive_chunk=chunk)
    endpoint = case.endpoint(host)
    guest = case.launch("guest", "guest", address=address, server=endpoint,
                        send_chunk=chunk, receive_chunk=chunk, exit_after_exchange=True)
    guest.wait_ready()
    guest_report = guest.finish(seconds=case.child_seconds)
    host_report = host.finish(seconds=case.child_seconds)
    check(host_report["pid"] != guest_report["pid"], "Two independent OS process identities")
    check(host_report["bound"] != guest_report["bound"], "Processes use distinct actual local endpoints")
    assert_exchange(host_report, role="host"); assert_exchange(guest_report, role="guest")
    for report in (host_report,guest_report):
        check(any(event.get("startToken") == 0x1234 for event in events(report,phase="Running")),
              "Running evidence retains the actual authorized start token")
    check(metric(host_report,"accepted") == 1 and metric(host_report,"refused") == 0,
          "One admitted guest, no accidental extra connection")
    if not preauthorize:
        for report in (host_report, guest_report):
            waiting = events(report, phase="WaitingForHost")
            running = events(report, phase="Running")
            check(waiting and running and waiting[0]["elapsedMs"] <= running[0]["elapsedMs"],
                  "Ready/wait precedes Running")
    if delay:
        check(events(host_report, phase="Running")[0]["elapsedMs"]
              - events(host_report, phase="WaitingForHost")[0]["elapsedMs"] >= delay,
              "Server start delay was observed while the client waited")
    if chunk == 1:
        check(metric(host_report,"partialFrameAdvances") > 0
              and metric(guest_report,"partialFrameAdvances") > 0,
              "One-byte real send calls advance a retained frame cursor")
    check(metric(host_report,"receiveWouldBlock") > 0 or metric(guest_report,"receiveWouldBlock") > 0,
          "Nonblocking real receive returned WouldBlock")
    case.observations.update(networkScope="same-host-loopback" if address.startswith("127.")
                             else "same-host-non-loopback-IPv4", endpoint=endpoint,
                             independentProcessPids=[host_report["pid"],guest_report["pid"]])


def reconnect_and_refuse_extra(case: Case) -> None:
    host = case.launch("host", "host", run_ms=3300, preauthorize=True, expect_connections=2)
    endpoint = case.endpoint(host)
    guest1 = case.launch("guest1", "guest", server=endpoint, run_ms=1000)
    guest1.wait_ready()
    # Establish an admitted first connection before probing the extra slot.
    time.sleep(0.25)
    with connect(endpoint) as extra:
        refused = drain_until_closed(extra, seconds=2)
    guest1_report = guest1.finish(seconds=case.child_seconds)
    assert_exchange(guest1_report, role="guest")
    # The listener remains bound while the next generation receives a new core.
    guest2 = case.launch("guest2", "guest", server=endpoint, exit_after_exchange=True)
    guest2.wait_ready()
    guest2_report = guest2.finish(seconds=case.child_seconds)
    host_report = host.finish(seconds=case.child_seconds)
    assert_exchange(guest2_report, role="guest"); assert_exchange(host_report, role="host", connections=2)
    check(metric(host_report,"accepted") == 2 and metric(host_report,"refused") >= 1,
          "Extra guest refused while first survives, then exactly one new generation admitted")
    check(metric(host_report,"connections") == 2, "Reconnect creates a fresh connection identity")
    case.observations.update(endpoint=endpoint, extraConnection=refused,
                             independentProcessPids=[host_report["pid"],guest1_report["pid"],guest2_report["pid"]])


def refused_connection(case: Case) -> None:
    # This socket owns the candidate port without listening. No preselected
    # shared user port is used and no reserve/close race occurs.
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as reservation:
        reservation.bind(("127.0.0.1",0))
        endpoint = reservation.getsockname()
        guest = case.launch("guest", "guest", server=endpoint, run_ms=4000,
                            echo_sizes=False, expect_connections=0)
        report = guest.finish(expected_exit=3,seconds=case.child_seconds)
    check(not events(report,phase="Running"), "Refused TCP connection never attaches a running session")
    check(socket_errors(report) & {10061,111}, "Actual OS connection-refused error retained")
    check(any(event.get("closeReason") in (3,"ConnectFailure")
              and event.get("socketError") in (10061,111) for event in report.get("events",[])),
          "Refusal is observed as a real pre-close connect failure, not forced run-limit cleanup")
    case.observations.update(endpoint=endpoint,actualSocketErrors=sorted(socket_errors(report)))


def bind_conflict(case: Case) -> None:
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as reservation:
        reservation.bind(("127.0.0.1",0)); reservation.listen(1)
        endpoint = reservation.getsockname()
        host = case.launch("host", "host", bind_port=endpoint[1], run_ms=500,
                           echo_sizes=False,expect_connections=0)
        report = host.finish(expected_exit=3,seconds=case.child_seconds)
    check(not events(report,phase="Running"), "Bind conflict does not create a session")
    check(socket_errors(report) & {10048,10013,98,13}, "Actual OS bind-conflict error retained")
    case.observations.update(endpoint=endpoint,actualSocketErrors=sorted(socket_errors(report)))


def release_and_rebind(case: Case) -> None:
    first = case.launch("host1", "host", run_ms=1500, preauthorize=True)
    endpoint = case.endpoint(first)
    guest = case.launch("guest", "guest",server=endpoint,exit_after_exchange=True)
    guest.wait_ready(); guest_report = guest.finish(seconds=case.child_seconds)
    report1 = first.finish(seconds=case.child_seconds)
    assert_exchange(guest_report,role="guest"); assert_exchange(report1,role="host")
    check(any(event.get("closeReason") in (2,"RemoteEof") for event in report1["events"]),
          "First active connection ended through actual peer EOF before listener release")
    second = case.launch("host2", "host", bind_port=endpoint[1], run_ms=250,
                         echo_sizes=False,expect_connections=0)
    endpoint2 = case.endpoint(second); report2 = second.finish(seconds=case.child_seconds)
    check(endpoint2 == endpoint, "A new process can bind the same released endpoint")
    check(report1["pid"] != report2["pid"], "Rebind belongs to a fresh OS process")
    case.observations.update(endpoint=endpoint,closureScope="active exchange/passive peer EOF/listener close/rebind",
                             independentProcessPids=[report1["pid"],guest_report["pid"],report2["pid"]])


def stress_would_block(case: Case) -> None:
    count = 4096
    host = case.launch("host", "host",run_ms=5000,preauthorize=True,
                       timeout_ms=6000,heartbeat_ms=100,echo_sizes=False,
                       extra_args=["--stress-messages",str(count),"--send-buffer","1024",
                                   "--pump-sleep-ms","0"])
    endpoint = case.endpoint(host)
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as peer:
        peer.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024)
        receive_buffer = peer.getsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF)
        peer.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
        peer.settimeout(3); peer.connect(endpoint)
        handshake_guest(peer,expected_token=0x1234)
        withheld_at = time.monotonic(); time.sleep(0.8)
        withheld_seconds = time.monotonic()-withheld_at
        actual_hash = hashlib.sha256(); expected_hash = hashlib.sha256()
        deadline = time.monotonic()+8
        for sequence in range(count):
            remaining = deadline-time.monotonic()
            check(remaining > 0,"Stress reader stays inside its total deadline")
            packet = receive_packet(peer,seconds=min(3,remaining))
            check((packet.source,packet.destination,packet.packet_type,packet.tcp,
                   packet.field1,packet.field2) == (0,1,2,1,STRESS_CLASS,sequence),
                  f"Exact stress header and FIFO sequence {sequence}")
            expected = bytes((256+index*17+sequence)&255 for index in range(256))
            check(packet.payload == expected,f"Exact actual stress payload {sequence}")
            actual_hash.update(packet.payload); expected_hash.update(expected)
    report = host.finish(seconds=case.child_seconds)
    check(report.get("probeErrors") == [], "OS backpressure did not kill the host service")
    check(report.get("stress") == {"requested":count,"queued":count},
          "Bounded producer queued the entire declared stress corpus")
    check(metric(report,"sendWouldBlock") > 0, "Real OS send returned WouldBlock under receiver backpressure")
    check(metric(report,"bytesSent") >= count*(17+256)+40,
          "Actual OS byte counter includes every validated frame and handshake replies")
    check(actual_hash.digest() == expected_hash.digest(), "Full independent received-stream digest matches")
    case.observations.update(endpoint=endpoint,frames=count,payloadBytes=count*256,
                             payloadSha256=actual_hash.hexdigest().upper(),
                             receiverBufferRequested=1024,receiverBufferActual=receive_buffer,
                             readingWithheldSeconds=withheld_seconds,
                             nativeSendWouldBlockQualified=True,
                             nativeShortSendQualified=metric(report,"shortSendResults") > 0,
                             producerSleepMilliseconds=0)


def fault(case: Case, kind: str) -> None:
    host = case.launch("host", "host", run_ms=1900, echo_sizes=False,
                       preauthorize=True,heartbeat_ms=100,timeout_ms=700)
    endpoint = case.endpoint(host)
    peer = connect(endpoint)
    observed: dict[str,Any] = {}
    try:
        if kind == "truncated-header":
            peer.sendall(PROBE[:10]); peer.shutdown(socket.SHUT_WR)
            observed = drain_until_closed(peer,seconds=2)
            expected_error = "FramingFailure"
        elif kind == "truncated-payload":
            handshake_guest(peer)
            peer.sendall(packet_bytes(1,0,2,b"12345")[:-3]); peer.shutdown(socket.SHUT_WR)
            observed = drain_until_closed(peer,seconds=2)
            expected_error = "FramingFailure"
        elif kind == "oversize257":
            peer.sendall(oversized_header()); observed = drain_until_closed(peer,seconds=2)
            expected_error = "FramingFailure"
        elif kind == "wrong-identity":
            handshake_guest(peer)
            peer.sendall(packet_bytes(0,0,2,b"bad"))
            observed = drain_until_closed(peer,seconds=2)
            expected_error = "InvalidIdentity"
        elif kind in ("prefix-then-eof","prefix-then-oversize"):
            handshake_guest(peer,expected_token=0x1234)
            prefix = packet_bytes(1,0,2,b"\x20",field1=GUEST_REQUEST,field2=1)
            tail = oversized_header() if kind == "prefix-then-oversize" else b""
            # One sendall submits a completed application frame before the
            # later bad header/EOF. TCP may split or coalesce this byte stream.
            peer.sendall(prefix+tail); peer.shutdown(socket.SHUT_WR)
            observed = drain_until_closed(peer,seconds=2)
            observed.update(applicationPrefixHex=prefix.hex(),tailBytes=len(tail))
            expected_error = "FramingFailure" if tail else None
        elif kind == "clean-eof":
            handshake_guest(peer); peer.shutdown(socket.SHUT_WR)
            observed = drain_until_closed(peer,seconds=2)
            expected_error = None
        elif kind == "reset":
            handshake_guest(peer); abort(peer)
            observed = {"resetRequested":True}; expected_error = "TransportFailure"
        elif kind == "timeout":
            handshake_guest(peer)
            observed = drain_until_closed(peer,seconds=2)
            expected_error = "Timeout"
        else:
            raise AssertionError(kind)
    finally:
        peer.close()
    report = host.finish(seconds=case.child_seconds)
    check(report.get("probeErrors") == [], "Peer fault did not kill the host service")
    check(metric(report,"accepted") == 1, "One real fault-peer connection accepted")
    if expected_error:
        check(events(report,error=expected_error), f"Specific peer failure {expected_error} retained")
    else:
        clean = [event for event in events(report,phase="Closed",error="None")
                 if int(event["connectionId"]) > 0 and event.get("closeReason") in (2,"RemoteEof")]
        check(clean and not events(report,error="TransportFailure") and not events(report,error="Timeout"),
              "Clean peer EOF remains distinct from transport failure or timeout")
    if kind == "reset":
        check(socket_errors(report) & {10054,10053,104,103}, "Actual reset error retained")
        check(metric(report,"bytesReceived") >= len(PROBE)+len(READY),
              "A positive receive prefix was processed before the later reset")
    if kind == "timeout":
        check(events(report,error="Timeout")[0]["elapsedMs"] >= 700, "Real elapsed timeout was observed")
    if kind in ("prefix-then-eof","prefix-then-oversize"):
        check(normalized_messages(report,1,"received") == [(GUEST_REQUEST,1,"20")],
              "Completed application prefix remains delivered with its original connection ID after the terminal tail")
        expected_bytes = len(PROBE)+len(READY)+18+(17 if kind == "prefix-then-oversize" else 0)
        check(metric(report,"bytesReceived") >= expected_bytes,
              "Actual receive bytes include the application prefix and complete submitted tail")
        check(any(event["connectionId"] == 1 for event in events(report,phase="Running")),
              "Application prefix belongs to the independently bootstrapped running connection")
    case.observations.update(endpoint=endpoint,fault=kind,faultPeer=observed,
                             actualSocketErrors=sorted(socket_errors(report)))


def reject_host_exit_after_exchange(case: Case) -> None:
    # Parse must reject this Guest-only option before Probe constructs a
    # transport. This case deliberately does not wait for ready/final JSON.
    host = case.launch("host", "host", preauthorize=True, exit_after_exchange=True,
                       run_ms=100, expect_connections=0)
    code = host.process.wait(timeout=case.child_seconds)
    host.stdout_handle.flush(); host.stderr_handle.flush()
    check(code == 3, "Host rejects Guest-only exit-after-exchange with the parse-error exit code")
    check(not host.ready_path.exists(), "Rejected arguments do not create transport-ready output")
    check(not host.report_path.exists(), "Rejected arguments do not create a transport report")
    diagnostic = host.stderr_path.read_text(encoding="utf-8")
    check("Invalid diagnostic configuration" in diagnostic, "Parse rejection has the expected diagnostic")
    case.observations.update(networkScope="argument-validation-before-transport-construction",
                             expectedExitCode=3, readyCreated=False, reportCreated=False,
                             stderr=diagnostic.strip())


CASES: dict[str,Callable[[Case],None]] = {
    "host-exit-after-exchange-rejected":reject_host_exit_after_exchange,
    "loopback-delayed-start":lambda case:exchange(case,delay=300),
    "loopback-preauthorized":lambda case:exchange(case,preauthorize=True),
    "loopback-one-byte-io":lambda case:exchange(case,chunk=1),
    "reconnect-refuse-extra":reconnect_and_refuse_extra,
    "connection-refused":refused_connection,
    "bind-conflict":bind_conflict,
    "release-rebind":release_and_rebind,
    "send-would-block":stress_would_block,
    **{name:lambda case,kind=name:fault(case,kind)
       for name in ("truncated-header","truncated-payload","oversize257","wrong-identity",
                    "prefix-then-eof","prefix-then-oversize","clean-eof","reset","timeout")},
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe",required=True,type=Path)
    parser.add_argument("--output",required=True,type=Path,help="Fresh directory, never reused")
    parser.add_argument("--case",action="append",choices=[*CASES,"non-loopback"])
    parser.add_argument("--non-loopback",help="Explicit local non-loopback IPv4 interface address")
    parser.add_argument("--child-seconds",type=float,default=10)
    args = parser.parse_args()
    executable = args.exe.resolve(); output = args.output.resolve()
    check(executable.is_file(), "Diagnostic executable exists")
    check(not output.exists(), "Fresh immutable output directory required")
    check(3 <= args.child_seconds <= 30, "Bounded child deadline is 3..30 seconds")
    names = args.case or [*CASES,*(["non-loopback"] if args.non_loopback else [])]
    check(len(names) == len(set(names)),"Each selected case has one fresh directory")
    if "non-loopback" in names:
        check(args.non_loopback is not None and not args.non_loopback.startswith("127."),
              "An explicit non-loopback interface is required")
        socket.inet_pton(socket.AF_INET,args.non_loopback)
        check(args.non_loopback not in ("0.0.0.0","255.255.255.255"),"Non-loopback test binds an actual unicast interface")
    output.mkdir(parents=True)
    sources = source_hashes(); executable_hash = sha(executable)
    report: dict[str,Any] = {"kind":"WinxCoop actual TCP cross-process verification", "passed":True,
        "executable":{"path":str(executable),"sha256":executable_hash},"sources":sources,
        "producerSha256":sha(Path(__file__)),"cases":[],
        "limits":{"maximumSimultaneousProbes":3,"childSeconds":args.child_seconds,
                  "socketReadDeadlineSeconds":3,"maximumFaultPeerReceivedBytes":8192,
                  "stressReaderTotalSeconds":8,"stressWithheldReadSeconds":0.8},
        "scope":{"realSockets":True,"independentProcesses":True,"guestOSIsolationProved":False,
                 "physicalSecondMachineProved":False,"gameLaunched":False,
                 "nonLoopbackSameHostIsNotTwoOSLAN":True}}
    started = time.monotonic()
    for name in names:
        directory = output/name; directory.mkdir()
        case = Case(executable,directory,args.child_seconds)
        entry: dict[str,Any] = {"name":name,"passed":False}
        case_started = time.monotonic()
        try:
            if name == "non-loopback":
                exchange(case,address=args.non_loopback,preauthorize=True)
            else:
                CASES[name](case)
            entry["passed"] = True
        except BaseException as error:
            entry["failure"] = repr(error); entry["traceback"] = traceback.format_exc()
            report["passed"] = False
        finally:
            case.close()
            entry.update(seconds=time.monotonic()-case_started,
                         observations=case.observations,probes=case.snapshots())
            report["cases"].append(entry)
            (output/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print(json.dumps({"case":name,"passed":entry["passed"],"failure":entry.get("failure")}),flush=True)
        if not entry["passed"]:
            break
    changed = [name for name,digest in sources.items() if sha(ROOT/name) != digest]
    if sha(executable) != executable_hash:
        changed.append(str(executable))
    report.update(frozenArtifactsUnchanged=not changed,changedArtifacts=changed,seconds=time.monotonic()-started)
    report["passed"] &= not changed
    (output/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"passed":report["passed"],"completedCases":len(report["cases"]),
                      "output":str(output/"report.json")}),flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
