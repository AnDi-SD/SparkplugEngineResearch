#!/usr/bin/env python3
"""Read-only full-corpus report for spNavigationGraph and path tables."""

from __future__ import annotations

import argparse
import collections
import hashlib
import itertools
import sqlite3
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_smo_mesh_navigation_set import (  # noqa: E402
    MESH_NAVIGATION_SET,
    NAVIGATION_GRAPH,
    NAVIGATION_PORTAL,
    canonical_path,
    decode_relationship,
    read_resource,
)
from analyze_smo_navigation_portal import load as load_portals  # noqa: E402


@dataclass(frozen=True)
class NavigationPath:
    source_set: int
    destination_set: int
    next_portal: int
    path_info: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class Graph:
    corpus: str
    file_id: int
    path: str
    object_index: int
    object_id: int
    name: str
    position: tuple[float, float, float]
    animated: bool
    node_sets: tuple[object, ...]
    sets: tuple[object, ...]
    portals: tuple[object, ...]
    table_size: int
    paths: tuple[NavigationPath, ...]
    serialized_hash: str


def decode_path(payload: bytes) -> NavigationPath:
    if len(payload) < 10:
        raise ValueError("short navigation path")
    source, destination = struct.unpack_from("<II", payload)
    next_portal, count = payload[8:10]
    if len(payload) != 10 + count * 2:
        raise ValueError("navigation path-info count does not match payload")
    info = tuple(
        (payload[offset], payload[offset + 1])
        for offset in range(10, len(payload), 2)
    )
    return NavigationPath(source, destination, next_portal, info)


def load(connection: sqlite3.Connection) -> list[Graph]:
    rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.object_id,
               o.name,c.source_kind,c.source_root,ct.relative_path,fo.byte_offset,
               f.byte_size
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inner_fo.id) FROM file_occurrences inner_fo
            WHERE inner_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index
        """, (NAVIGATION_GRAPH,),
    ).fetchall()
    resources: dict[int, bytes] = {}
    object_maps: dict[int, dict[int, tuple[int, int, str]]] = {}
    result: list[Graph] = []
    for (
        corpus, file_id, path, index, object_id, name, source_kind, source_root,
        container_path, occurrence_offset, byte_size,
    ) in rows:
        if file_id not in resources:
            resources[file_id] = read_resource(
                source_kind, source_root, path, container_path,
                occurrence_offset, byte_size,
            )
            object_maps[file_id] = {
                row_id: (row_index, row_type, row_name)
                for row_index, row_id, row_type, row_name in connection.execute(
                    """SELECT object_index,object_id,type_hash,name FROM objects
                       WHERE file_id=?""", (file_id,)
                )
            }
        resource = resources[file_id]
        db_fields = connection.execute(
            """SELECT section_index,field_type,payload_size,payload_preview,
                      absolute_payload_offset
               FROM direct_fields WHERE file_id=? AND object_index=?
                 AND is_section_terminator=0 ORDER BY field_index""",
            (file_id, index),
        ).fetchall()
        fields: dict[tuple[int, int, int], bytes] = {}
        occurrences: collections.Counter[tuple[int, int]] = collections.Counter()
        serialized = bytearray()
        for section, field_type, size, preview, offset in db_fields:
            payload = resource[offset:offset + size]
            if len(payload) != size or payload[:len(preview)] != bytes(preview):
                raise ValueError("source graph field does not match database")
            key = section, field_type
            occurrence = occurrences[key]
            occurrences[key] += 1
            fields[(section, field_type, occurrence)] = payload
            serialized += struct.pack("<III", section, field_type, size) + payload
        if (2, 2, 0) not in fields or len(fields[(2, 2, 0)]) != 4:
            raise ValueError("graph has no UInt32 path table size")
        unexpected = [
            key for key in fields
            if not ((key[0] == 0 and key[1] in range(9)) or
                    (key[0] == 2 and key[1] in range(4)))
        ]
        if unexpected or any(key[0] == 1 for key in fields):
            raise ValueError(f"unexpected graph fields {unexpected}")
        object_map = object_maps[file_id]
        node_sets = tuple(
            decode_relationship(fields[(0, 5, occurrence)], object_map)
            for occurrence in range(occurrences[(0, 5)])
        )
        sets = tuple(
            decode_relationship(fields[(2, 0, occurrence)], object_map)
            for occurrence in range(occurrences[(2, 0)])
        )
        portals = tuple(
            decode_relationship(fields[(2, 1, occurrence)], object_map)
            for occurrence in range(occurrences[(2, 1)])
        )
        table_size = struct.unpack("<I", fields[(2, 2, 0)])[0]
        paths = tuple(
            decode_path(fields[(2, 3, occurrence)])
            for occurrence in range(occurrences[(2, 3)])
        )
        if not sets or table_size != len(sets) or len(paths) != table_size ** 2:
            raise ValueError("graph set count or square path table is inconsistent")
        if tuple((value.source_set, value.destination_set) for value in paths) != \
                tuple(itertools.product(range(table_size), repeat=2)):
            raise ValueError("graph paths are not stored in row-major set order")
        if any(value.target_type != MESH_NAVIGATION_SET for value in sets):
            raise ValueError("graph set relationship has wrong type")
        if any(value.target_type != NAVIGATION_PORTAL for value in portals):
            raise ValueError("graph portal relationship has wrong type")
        if set(value.object_id for value in node_sets) != \
                set(value.object_id for value in sets + portals):
            raise ValueError("node children and graph-owned objects differ")
        position_payload = fields.get((0, 0, 0), bytes(12))
        animated_payload = fields.get((0, 8, 0), b"\0")
        if len(position_payload) != 12 or animated_payload not in (b"\0", b"\1"):
            raise ValueError("invalid graph node values")
        result.append(Graph(
            corpus, file_id, path, index, object_id, name,
            struct.unpack("<3f", position_payload), bool(animated_payload[0]),
            node_sets, sets, portals, table_size, paths,
            hashlib.sha256(serialized).hexdigest(),
        ))
    return result


def key(items: list[Graph], corpus: str):
    return {
        canonical_path(item.path): item
        for item in items if item.corpus == corpus
    }


def core(item: Graph):
    return (
        item.position, item.animated, item.table_size, item.paths,
        tuple(value.target_type for value in item.sets),
        tuple(value.target_type for value in item.portals),
    )


def validate_cross_references(
    connection: sqlite3.Connection, items: list[Graph]
) -> collections.Counter[str]:
    """Prove the graph table against the portal endpoints and portal path rows."""
    portals = {
        (item.file_id, item.object_id): item for item in load_portals(connection)
    }
    stats: collections.Counter[str] = collections.Counter()
    for item in items:
        set_ids = tuple(value.object_id for value in item.sets)
        portal_items = tuple(
            portals[(item.file_id, value.object_id)] for value in item.portals
        )
        for portal in portal_items:
            if portal.graph.object_id != item.object_id:
                raise ValueError("portal points at a different navigation graph")
            if any(value.object_id not in set_ids for value in portal.sets):
                raise ValueError("portal endpoint is absent from its graph set list")
        for path in item.paths:
            stats["path_rows"] += 1
            if any(portal_index >= len(portal_items)
                   for portal_index, _ in path.path_info):
                raise ValueError("path-info portal index exceeds graph portal list")
            if path.path_info:
                stats["rows_with_path_info"] += 1
                if path.next_portal == path.path_info[0][0]:
                    stats["next_portal_equals_first_path_info"] += 1
            else:
                stats["rows_without_path_info"] += 1
            for path_index, (portal_index, auxiliary) in enumerate(path.path_info):
                stats["path_info_pairs"] += 1
                portal = portal_items[portal_index]
                if (path.source_set, path.destination_set, path_index) \
                        not in portal.paths:
                    raise ValueError(
                        "graph path variant does not start with its declared portal: "
                        f"{item.corpus}:{item.path} {path.source_set}->"
                        f"{path.destination_set}, portal={portal_index}, "
                        f"path={path_index}, auxiliary={auxiliary}")
                stats["path_info_pairs_matched_to_portal"] += 1

                route_portals = tuple(
                    index for index, candidate in enumerate(portal_items)
                    if (path.source_set, path.destination_set, path_index)
                    in candidate.paths
                )
                adjacency: dict[int, list[tuple[int, int]]] = collections.defaultdict(list)
                for route_portal_index in route_portals:
                    endpoints = tuple(
                        set_ids.index(value.object_id)
                        for value in portal_items[route_portal_index].sets
                    )
                    adjacency[endpoints[0]].append((endpoints[1], route_portal_index))
                    adjacency[endpoints[1]].append((endpoints[0], route_portal_index))
                if portal_index not in route_portals or not route_portals:
                    raise ValueError("path variant has no portal membership rows")
                if any(len(edges) > 2 for edges in adjacency.values()):
                    raise ValueError("path variant portal membership branches")
                current = path.source_set
                previous_portal = None
                used: set[int] = set()
                while current != path.destination_set:
                    choices = [edge for edge in adjacency[current]
                               if edge[1] != previous_portal]
                    if len(choices) != 1:
                        raise ValueError("path variant portal membership is disconnected")
                    next_set, selected_portal = choices[0]
                    if selected_portal in used:
                        raise ValueError("path variant portal membership contains a cycle")
                    if not used and selected_portal != portal_index:
                        raise ValueError("path-info portal is not the first route portal")
                    used.add(selected_portal)
                    previous_portal = selected_portal
                    current = next_set
                if used != set(route_portals):
                    raise ValueError("path variant contains portals beyond its destination")
                stats["alternative_routes_verified"] += 1

            if path.source_set == path.destination_set:
                if path.next_portal != 0xFF or path.path_info:
                    raise ValueError("diagonal graph route is not the empty sentinel")
                stats["diagonal_empty_routes"] += 1
                continue
            if path.next_portal == 0xFF:
                if path.path_info:
                    raise ValueError("unreachable graph route contains path info")
                stats["unreachable_routes"] += 1
                continue
            if path.next_portal >= len(portal_items) or not path.path_info:
                raise ValueError("reachable graph route has an invalid next portal")

            current = path.source_set
            visited: set[int] = set()
            for _ in range(len(item.sets)):
                if current == path.destination_set:
                    break
                if current in visited:
                    raise ValueError("navigation graph route contains a cycle")
                visited.add(current)
                step = item.paths[current * item.table_size + path.destination_set]
                if step.next_portal >= len(portal_items):
                    raise ValueError("navigation graph route terminates before destination")
                endpoint_ids = tuple(
                    value.object_id for value in portal_items[step.next_portal].sets
                )
                current_id = set_ids[current]
                if current_id not in endpoint_ids or endpoint_ids[0] == endpoint_ids[1]:
                    raise ValueError("next portal is not incident to the current set")
                next_id = endpoint_ids[1] if endpoint_ids[0] == current_id else endpoint_ids[0]
                current = set_ids.index(next_id)
            if current != path.destination_set:
                raise ValueError("navigation graph route did not reach its destination")
            stats["reachable_routes_verified"] += 1
        expected_rows = {
            (path.source_set, path.destination_set, path_index)
            for path in item.paths for path_index in range(len(path.path_info))
        }
        actual_rows = {
            row for portal in portal_items for row in portal.paths
        }
        if expected_rows != actual_rows:
            raise ValueError("portal path rows do not invert the graph alternatives")
        if sum(len(portal.paths) for portal in portal_items) < len(expected_rows):
            raise ValueError("portal membership total is smaller than path variants")
        stats["portal_membership_rows"] += sum(
            len(portal.paths) for portal in portal_items)
        stats["distinct_path_variants"] += len(expected_rows)
    return stats


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(args.database)
    items = load(connection)
    cross_reference_stats = validate_cross_references(connection, items)
    print("objects", len(items))
    for corpus, iterator in itertools.groupby(items, lambda item: item.corpus):
        group = list(iterator)
        info_counts = collections.Counter(
            len(path.path_info) for item in group for path in item.paths
        )
        print(
            corpus, "objects", len(group),
            "sets", (min(x.table_size for x in group),
                     max(x.table_size for x in group)),
            "portals", (min(len(x.portals) for x in group),
                        max(len(x.portals) for x in group)),
            "paths", sum(len(x.paths) for x in group),
            "path-info-counts", info_counts,
            "next-portals", collections.Counter(
                path.next_portal for item in group for path in item.paths),
            "info-byte0", collections.Counter(
                pair[0] for item in group for path in item.paths
                for pair in path.path_info),
            "info-byte1", collections.Counter(
                pair[1] for item in group for path in item.paths
                for pair in path.path_info),
            "node animated", collections.Counter(x.animated for x in group),
        )
    for left_name, right_name in (
        ("pc-working", "pc-pristine"),
        ("pc-pristine", "ps2-pristine"),
    ):
        left, right = key(items, left_name), key(items, right_name)
        keys = left.keys() & right.keys()
        print(
            "compare", left_name, right_name, "paired", len(keys),
            "core", sum(core(left[k]) == core(right[k]) for k in keys),
            "bytes", sum(left[k].serialized_hash == right[k].serialized_hash
                         for k in keys),
            "left_only", len(left.keys() - right.keys()),
            "right_only", len(right.keys() - left.keys()),
        )
    print("cross-reference validation", cross_reference_stats)
    print("examples")
    for item in (value for value in items if value.corpus == "pc-pristine"):
        print(
            canonical_path(item.path), "sets", item.table_size,
            "portals", len(item.portals),
            "path_info", sum(len(value.path_info) for value in item.paths),
            "first", item.paths[:min(6, len(item.paths))],
        )
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
