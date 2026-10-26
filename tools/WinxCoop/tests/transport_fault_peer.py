"""Independent, test-only wire fixtures and bounded real-socket fault peers.

Production serialization remains in the shared Sparkplug codec. These literal
fixtures deliberately do not call SessionCore or its encoder, so process tests
can detect a mutually consistent but incorrect implementation.
"""
from __future__ import annotations

from dataclasses import dataclass
import os
import socket
import struct
import time


HEADER = struct.Struct("<HHHBIIH")
MAXIMUM_PAYLOAD = 256
PROBE = bytes.fromhex("d0 ba 00 00 00 00 01 00 00 00 00 00 00 00 00 01 00 2a")
INFO = bytes.fromhex("00 00 ff ff 01 00 01 00 00 00 00 00 00 00 00 02 00 01 00")
READY = bytes.fromhex("01 00 00 00 03 00 01 00 00 00 00 00 00 00 00 01 00 01")


@dataclass(frozen=True)
class Packet:
    source: int
    destination: int
    packet_type: int
    tcp: int
    field1: int
    field2: int
    payload: bytes


def packet_bytes(source: int, destination: int, packet_type: int,
                 payload: bytes = b"", *, tcp: int = 1,
                 field1: int = 0, field2: int = 0) -> bytes:
    """Test-only fault input; accepts oversize specifically to test rejection."""
    if len(payload) > 65535:
        raise ValueError("A wire size is uint16")
    return HEADER.pack(source, destination, packet_type, tcp,
                       field1, field2, len(payload)) + payload


def oversized_header(size: int = 257) -> bytes:
    if not MAXIMUM_PAYLOAD < size <= 65535:
        raise ValueError("An oversized test header must exceed the codec allocation")
    return HEADER.pack(1, 0, 2, 1, 0, 0, size)


def _read_exact(connection: socket.socket, size: int, deadline: float) -> bytes:
    result = bytearray()
    while len(result) < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"Timed out after {len(result)}/{size} frame bytes")
        connection.settimeout(min(remaining, 0.25))
        try:
            data = connection.recv(size - len(result))
        except socket.timeout:
            continue
        if not data:
            raise EOFError(f"EOF after {len(result)}/{size} frame bytes")
        result.extend(data)
    return bytes(result)


def receive_packet(connection: socket.socket, *, seconds: float = 3.0) -> Packet:
    deadline = time.monotonic() + seconds
    header = HEADER.unpack(_read_exact(connection, HEADER.size, deadline))
    size = header[-1]
    if size > MAXIMUM_PAYLOAD:
        raise AssertionError(f"Probe emitted an oversized packet: {size}")
    return Packet(*header[:-1], _read_exact(connection, size, deadline))


def connect(endpoint: tuple[str, int], *, seconds: float = 3.0) -> socket.socket:
    connection = socket.create_connection(endpoint, timeout=seconds)
    connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return connection


def handshake_guest(connection: socket.socket, *, seconds: float = 3.0,
                    expected_token: int | None = None) -> Packet:
    connection.sendall(PROBE)
    reply = receive_packet(connection, seconds=seconds)
    expected = Packet(0, 65535, 1, 1, 0, 0, b"\x01\x00")
    if reply != expected:
        raise AssertionError(f"Independent InitialInfoReply mismatch: {reply!r}")
    connection.sendall(READY)
    start = receive_packet(connection, seconds=seconds)
    if (start.source, start.destination, start.packet_type, start.tcp,
            start.field1, start.field2) != (0, 1, 4, 1, 0, 0):
        raise AssertionError(f"Independent GameStart header mismatch: {start!r}")
    if len(start.payload) != 4 or start.payload[:2] != b"\x01\x00":
        raise AssertionError(f"Independent GameStart payload mismatch: {start!r}")
    token = int.from_bytes(start.payload[2:], "little")
    if not token or (expected_token is not None and token != expected_token):
        raise AssertionError(f"Unexpected GameStart token: {token}")
    return start


def drain_until_closed(connection: socket.socket, *, seconds: float = 3.0,
                       maximum_bytes: int = 8192) -> dict[str, int | bool | None]:
    deadline = time.monotonic() + seconds
    total = 0
    while time.monotonic() < deadline:
        connection.settimeout(min(0.25, max(0.001, deadline - time.monotonic())))
        try:
            data = connection.recv(min(1024, maximum_bytes + 1 - total))
        except socket.timeout:
            continue
        except (ConnectionResetError, ConnectionAbortedError) as error:
            return {"closed": True, "reset": True, "receivedBytes": total,
                    "errorCode": getattr(error, "winerror", None) or error.errno}
        if not data:
            return {"closed": True, "reset": False, "receivedBytes": total,
                    "errorCode": None}
        total += len(data)
        if total > maximum_bytes:
            raise AssertionError("Fault peer received beyond its declared byte bound")
    raise TimeoutError("Peer was not closed before the fault deadline")


def abort(connection: socket.socket) -> None:
    """Close only this owned socket with SO_LINGER=0 (a real TCP reset)."""
    linger = struct.pack("HH", 1, 0) if os.name == "nt" else struct.pack("ii", 1, 0)
    connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
    connection.close()
