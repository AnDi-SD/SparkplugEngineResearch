#!/usr/bin/env python3
"""Read-only corpus report for spMeshNavigationSet in schema v2."""

from __future__ import annotations

import argparse
import collections
import hashlib
import itertools
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


MESH_NAVIGATION_SET = 0x7297173C
NAVIGATION_GRAPH = 0x188A161F
NAVIGATION_PORTAL = 0x385662AA
MESH_BV = 0x3F453DE7


@dataclass(frozen=True)
class Relationship:
    object_id: int
    inline_size: int | None
    encoding: str
    target_index: int | None
    target_type: int | None
    target_name: str | None


@dataclass(frozen=True)
class NavigationSet:
    corpus: str
    file_id: int
    path: str
    object_index: int
    object_id: int
    name: str
    parent_index: int | None
    parent_type: int | None
    parent_name: str | None
    serialized_size: int
    position: tuple[float, float, float]
    animated: bool
    node_count: int
    transition_rows: int
    transition_columns: int
    transition_values: tuple[int, ...]
    portal_transition_rows: int
    portal_transition_columns: int
    portal_transition_values: tuple[int, ...]
    links: tuple[tuple[int, tuple[int, ...]], ...]
    shared_edge_links: int
    manual_links: int
    omitted_shared_edges: int
    portals: tuple[Relationship, ...]
    enabled: bool
    mesh: Relationship
    serialized_hash: str


def canonical_path(path: str) -> str:
    value = path.replace("\\", "/").lower()
    return value[5:] if value.startswith("data/") else value


def read_resource(
    source_kind: str,
    source_root: str,
    relative_path: str,
    container_path: str | None,
    occurrence_offset: int | None,
    byte_size: int,
) -> bytes:
    root = Path(source_root)
    if source_kind == "directory":
        return (root / Path(relative_path.replace("/", "\\"))).read_bytes()
    if container_path is None or occurrence_offset is None:
        raise ValueError(f"incomplete PCK location for {relative_path}")
    archive = root / Path(container_path.replace("/", "\\"))
    with archive.open("rb") as stream:
        stream.seek(occurrence_offset)
        result = stream.read(byte_size)
    if len(result) != byte_size:
        raise ValueError(f"short PCK read for {relative_path}")
    return result


def decode_matrix(payload: bytes, label: str) -> tuple[int, int, tuple[int, ...]]:
    if len(payload) < 8:
        raise ValueError(f"short {label}")
    rows, columns = struct.unpack_from("<II", payload)
    count = rows * columns
    if len(payload) != 8 + count * 4:
        raise ValueError(
            f"invalid {label} length {len(payload)} for {rows}x{columns} UInt32")
    return rows, columns, struct.unpack_from(f"<{count}I", payload, 8)


def decode_links(payload: bytes) -> tuple[tuple[int, tuple[int, ...]], ...]:
    if len(payload) < 4:
        raise ValueError("short navigation links table")
    node_count = struct.unpack_from("<I", payload)[0]
    cursor = 4
    result: list[tuple[int, tuple[int, ...]]] = []
    for _ in range(node_count):
        if cursor + 5 > len(payload):
            raise ValueError("truncated navigation link record")
        node_id = payload[cursor]
        link_count = struct.unpack_from("<I", payload, cursor + 1)[0]
        cursor += 5
        if link_count > len(payload) - cursor:
            raise ValueError("navigation link list exceeds its field")
        links = tuple(payload[cursor : cursor + link_count])
        cursor += link_count
        result.append((node_id, links))
    if cursor != len(payload):
        raise ValueError("navigation links table has trailing bytes")
    return tuple(result)


def decode_mesh_geometry(payload: bytes) -> tuple[
    tuple[tuple[int, int, int], ...],
    tuple[tuple[float, float, float], ...],
]:
    if len(payload) < 12:
        raise ValueError("short spMeshBV geometry")
    version, triangle_count, reserved = struct.unpack_from("<III", payload)
    if version != 2 or reserved != 0 or len(payload) < 12 + triangle_count * 6:
        raise ValueError("invalid spMeshBV geometry header")
    index_count = triangle_count * 3
    indices = struct.unpack_from(f"<{index_count}H", payload, 12)
    cursor = 12 + index_count * 2
    if cursor + 12 > len(payload):
        raise ValueError("short spMeshBV vertex header")
    index_tail, vertex_count, vertex_reserved = struct.unpack_from(
        "<III", payload, cursor)
    cursor += 12
    if index_tail != 0 or vertex_reserved != 0 or len(payload) != cursor + vertex_count * 12:
        raise ValueError("invalid spMeshBV vertex payload")
    positions = tuple(
        struct.unpack_from("<3f", payload, cursor + index * 12)
        for index in range(vertex_count)
    )
    if any(index >= vertex_count for index in indices):
        raise ValueError("spMeshBV triangle index exceeds the vertex array")
    triangles = tuple(
        tuple(indices[offset : offset + 3])
        for offset in range(0, len(indices), 3)
    )
    return triangles, positions


def triangle_adjacency(
    triangles: tuple[tuple[int, int, int], ...],
    positions: tuple[tuple[float, float, float], ...],
) -> tuple[frozenset[int], ...]:
    edges: dict[
        tuple[tuple[float, float, float], tuple[float, float, float]],
        list[int],
    ] = collections.defaultdict(list)
    for triangle_index, triangle in enumerate(triangles):
        for start, end in zip(triangle, triangle[1:] + triangle[:1]):
            edge = tuple(sorted((positions[start], positions[end])))
            edges[edge].append(triangle_index)
    result: list[set[int]] = [set() for _ in triangles]
    for owners in edges.values():
        if len(owners) == 2:
            left, right = owners
            result[left].add(right)
            result[right].add(left)
    return tuple(frozenset(value) for value in result)


def decode_relationship(
    payload: bytes,
    objects_by_id: dict[int, tuple[int, int, str]],
) -> Relationship:
    if len(payload) == 4:
        object_id = struct.unpack_from("<I", payload)[0]
        inline_size = None
        encoding = "id_only"
    elif len(payload) >= 8:
        object_id, inline_size = struct.unpack_from("<II", payload)
        if inline_size != len(payload) - 8:
            raise ValueError(
                f"relationship inline size {inline_size} does not match {len(payload) - 8}")
        encoding = "sized_reference" if inline_size == 0 else "inline"
    else:
        raise ValueError(f"invalid relationship payload size {len(payload)}")
    target = objects_by_id.get(object_id)
    return Relationship(
        object_id,
        inline_size,
        encoding,
        target[0] if target else None,
        target[1] if target else None,
        target[2] if target else None,
    )


def decode_navigation_sets(connection: sqlite3.Connection) -> list[NavigationSet]:
    rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.object_id,
               o.name,o.parent_index,p.type_hash,p.name,o.serialized_size,
               c.source_kind,c.source_root,ct.relative_path,fo.byte_offset,
               f.byte_size
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects p ON p.file_id=o.file_id AND p.object_index=o.parent_index
        LEFT JOIN file_occurrences fo ON fo.id=(
            SELECT MIN(inner_fo.id) FROM file_occurrences inner_fo
            WHERE inner_fo.file_id=f.id)
        LEFT JOIN containers ct ON ct.id=fo.container_id
        WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index
        """,
        (MESH_NAVIGATION_SET,),
    ).fetchall()
    result: list[NavigationSet] = []
    resources: dict[int, bytes] = {}
    object_maps: dict[int, dict[int, tuple[int, int, str]]] = {}
    for (
        corpus, file_id, path, index, object_id, name, parent_index, parent_type,
        parent_name, size, source_kind, source_root, container_path,
        occurrence_offset, byte_size,
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
                       WHERE file_id=?""",
                    (file_id,),
                )
            }
        resource = resources[file_id]
        fields = connection.execute(
            """
            SELECT section_index,field_type,occurrence,payload_size,payload_preview,
                   absolute_payload_offset
            FROM direct_fields WHERE file_id=? AND object_index=?
              AND is_section_terminator=0 ORDER BY field_index
            """,
            (file_id, index),
        ).fetchall()
        decoded: dict[tuple[int, int, int], bytes] = {}
        serialized = bytearray()
        local_occurrences: collections.Counter[tuple[int, int]] = collections.Counter()
        for section, field_type, _occurrence, payload_size, preview, offset in fields:
            payload = resource[offset : offset + payload_size]
            if len(payload) != payload_size:
                raise ValueError("navigation payload exceeds the source resource")
            if payload[: len(preview)] != bytes(preview):
                raise ValueError("source payload no longer matches the database preview")
            local_key = (section, field_type)
            occurrence = local_occurrences[local_key]
            local_occurrences[local_key] += 1
            decoded[(section, field_type, occurrence)] = payload
            serialized.extend(struct.pack("<III", section, field_type, payload_size))
            serialized.extend(payload)

        required = [(0, 0, 0), (1, 0, 0), (1, 1, 0),
                    (1, 2, 0), (1, 3, 0), (1, 5, 0), (2, 0, 0)]
        if any(key not in decoded for key in required):
            raise ValueError(
                f"incomplete spMeshNavigationSet {corpus}:{path} [{index}]: "
                f"{sorted(decoded)}")
        unexpected = [
            key for key in decoded
            if not (key in required or
                    (key[0] == 0 and key[1] in (1, 2, 8)) or
                    (key[0] == 1 and key[1] == 4))
        ]
        if unexpected:
            raise ValueError(f"unexpected navigation fields {unexpected}")

        position = struct.unpack("<3f", decoded[(0, 0, 0)])
        animated_payload = decoded.get((0, 8, 0), b"\0")
        node_count_payload = decoded[(1, 0, 0)]
        enabled_payload = decoded[(1, 5, 0)]
        if len(animated_payload) != 1 or len(node_count_payload) != 4 or len(enabled_payload) != 1:
            raise ValueError("invalid fixed navigation field size")
        node_count = struct.unpack("<I", node_count_payload)[0]
        transition = decode_matrix(decoded[(1, 1, 0)], "transition table")
        portal_transition = decode_matrix(
            decoded[(1, 2, 0)], "portal transition table")
        links = decode_links(decoded[(1, 3, 0)])
        portals = tuple(
            decode_relationship(decoded[(1, 4, occurrence)], object_maps[file_id])
            for occurrence in range(sum(
                key[0] == 1 and key[1] == 4 for key in decoded))
        )
        mesh = decode_relationship(decoded[(2, 0, 0)], object_maps[file_id])

        if transition[:2] != (node_count, node_count):
            raise ValueError("transition matrix is not NodeCount x NodeCount")
        if portal_transition[:2] != (len(portals), node_count):
            raise ValueError(
                f"portal transition matrix {portal_transition[:2]} is not "
                f"PortalCount x NodeCount ({len(portals)}, {node_count}) in "
                f"{corpus}:{path} [{index}]")
        if len(links) != node_count:
            raise ValueError("links table count does not equal NodeCount")
        if tuple(item[0] for item in links) != tuple(range(node_count)):
            raise ValueError("links table node IDs are not dense and ordered")
        if any(link >= node_count for _, neighbours in links for link in neighbours):
            raise ValueError("links table contains an out-of-range node ID")
        if any(item.target_type != NAVIGATION_PORTAL for item in portals):
            raise ValueError("navigation portal relationship has the wrong target type")
        if mesh.target_type != MESH_BV:
            raise ValueError("navigation mesh relationship has the wrong target type")
        if mesh.encoding == "inline" and mesh.target_index is not None:
            target_parent = connection.execute(
                """SELECT parent_index FROM objects
                   WHERE file_id=? AND object_index=?""",
                (file_id, mesh.target_index),
            ).fetchone()[0]
            if target_parent != index:
                raise ValueError("inline navigation mesh is not its physical child")
        mesh_field = connection.execute(
            """SELECT payload_size,absolute_payload_offset FROM direct_fields
               WHERE file_id=? AND object_index=? AND section_index=0
                 AND field_type=0 AND is_section_terminator=0
               ORDER BY field_index LIMIT 1""",
            (file_id, mesh.target_index),
        ).fetchone()
        if mesh_field is None:
            raise ValueError("navigation spMeshBV has no geometry field")
        mesh_size, mesh_offset = mesh_field
        triangles, positions = decode_mesh_geometry(
            resource[mesh_offset : mesh_offset + mesh_size])
        if len(triangles) != node_count:
            raise ValueError("NodeCount does not equal navigation mesh triangle count")
        adjacency = triangle_adjacency(triangles, positions)
        total_links = sum(len(neighbours) for _, neighbours in links)
        shared_edge_links = sum(
            len(frozenset(neighbours) & adjacency[node])
            for node, neighbours in links
        )
        geometric_edge_links = sum(map(len, adjacency))

        result.append(NavigationSet(
            corpus, file_id, path, index, object_id, name, parent_index,
            parent_type, parent_name, size, position, bool(animated_payload[0]),
            node_count, *transition, *portal_transition, links,
            shared_edge_links, total_links - shared_edge_links,
            geometric_edge_links - shared_edge_links, portals,
            bool(enabled_payload[0]), mesh,
            hashlib.sha256(serialized).hexdigest(),
        ))
    return result


def semantic_signature(item: NavigationSet) -> tuple[object, ...]:
    relationship = lambda value: (
        value.object_id, value.inline_size, value.encoding,
        value.target_type, value.target_name,
    )
    return (
        item.name, item.position, item.animated, item.node_count,
        item.transition_rows, item.transition_columns, item.transition_values,
        item.portal_transition_rows, item.portal_transition_columns,
        item.portal_transition_values, item.links, item.shared_edge_links,
        item.manual_links, item.omitted_shared_edges,
        tuple(map(relationship, item.portals)), item.enabled,
        relationship(item.mesh),
    )


def core_signature(item: NavigationSet) -> tuple[object, ...]:
    return (
        item.position, item.animated, item.node_count,
        item.transition_rows, item.transition_columns, item.transition_values,
        item.portal_transition_rows, item.portal_transition_columns,
        item.portal_transition_values, item.links, item.shared_edge_links,
        item.manual_links, item.omitted_shared_edges, item.enabled,
        tuple(value.target_type for value in item.portals), item.mesh.target_type,
    )


def valid_transition_routes(item: NavigationSet) -> tuple[int, int]:
    links = dict(item.links)
    valid = total = 0
    for source in range(item.node_count):
        reachable = {source}
        frontier = [source]
        while frontier:
            current = frontier.pop()
            for neighbour in links[current]:
                if neighbour not in reachable:
                    reachable.add(neighbour)
                    frontier.append(neighbour)
        for destination in range(item.node_count):
            if source == destination or destination not in reachable:
                continue
            total += 1
            current = source
            visited: set[int] = set()
            while current != destination and current not in visited:
                visited.add(current)
                neighbours = links[current]
                selector = item.transition_values[
                    current * item.node_count + destination]
                if selector >= len(neighbours):
                    break
                current = neighbours[selector]
            valid += current == destination
    return valid, total


def keyed(items: list[NavigationSet], corpus: str):
    ordinals: collections.Counter[tuple[str, str]] = collections.Counter()
    result = {}
    for item in (value for value in items if value.corpus == corpus):
        base = (canonical_path(item.path), item.name.lower())
        ordinal = ordinals[base]
        ordinals[base] += 1
        result[(base[0], base[1], ordinal)] = item
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(f"file:{args.database.resolve()}?mode=ro", uri=True)
    items = decode_navigation_sets(connection)

    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size),SUM(o.name<>'')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=? GROUP BY c.id ORDER BY c.id
        """,
        (MESH_NAVIGATION_SET,),
    ):
        print(row)

    print("\nlayout")
    for corpus, group_iterator in itertools.groupby(items, lambda item: item.corpus):
        group = list(group_iterator)
        print(
            corpus,
            "nodes", (min(x.node_count for x in group), max(x.node_count for x in group)),
            "portals", dict(collections.Counter(len(x.portals) for x in group)),
            "enabled", dict(collections.Counter(x.enabled for x in group)),
            "animated", dict(collections.Counter(x.animated for x in group)),
            "mesh_encoding", dict(collections.Counter(x.mesh.encoding for x in group)),
            "portal_encoding", dict(collections.Counter(
                relationship.encoding for x in group for relationship in x.portals)),
        )

    print("\ngraph data")
    for corpus, group_iterator in itertools.groupby(items, lambda item: item.corpus):
        group = list(group_iterator)
        transition_values = collections.Counter(
            value for x in group for value in x.transition_values)
        portal_values = collections.Counter(
            value for x in group for value in x.portal_transition_values)
        links = [len(neighbours) for x in group for _, neighbours in x.links]
        reciprocal = sum(
            node in dict(x.links).get(target, ())
            for x in group for node, neighbours in x.links for target in neighbours)
        total_links = sum(links)
        route_counts = [valid_transition_routes(x) for x in group]
        print(
            corpus,
            "transition_values", transition_values.most_common(12),
            "portal_values", portal_values.most_common(12),
            "links", total_links,
            "degree", (min(links), max(links)),
            "reciprocal", f"{reciprocal}/{total_links}",
            "routes", f"{sum(x[0] for x in route_counts)}/"
                      f"{sum(x[1] for x in route_counts)}",
            "shared_edge_links", sum(x.shared_edge_links for x in group),
            "manual_links", sum(x.manual_links for x in group),
            "omitted_shared_edges", sum(x.omitted_shared_edges for x in group),
        )

    print("\nphysical hierarchy")
    for corpus, parent, count in connection.execute(
        """
        SELECT c.corpus_key,COALESCE(pc.engine_name,'<root>') AS parent_name,COUNT(*)
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects p ON p.file_id=o.file_id AND p.object_index=o.parent_index
        LEFT JOIN classes pc ON pc.type_hash=p.type_hash
        WHERE o.type_hash=? GROUP BY c.id,p.type_hash ORDER BY c.id,parent_name
        """,
        (MESH_NAVIGATION_SET,),
    ):
        print(corpus, parent, count)

    print("\ncross corpus")
    for left_name, right_name in (
        ("pc-working", "pc-pristine"),
        ("pc-pristine", "ps2-pristine"),
    ):
        left, right = keyed(items, left_name), keyed(items, right_name)
        keys = left.keys() & right.keys()
        print(
            left_name, right_name,
            "paired", len(keys),
            "semantic", sum(
                semantic_signature(left[key]) == semantic_signature(right[key])
                for key in keys),
            "core", sum(
                core_signature(left[key]) == core_signature(right[key])
                for key in keys),
            "field_bytes", sum(
                left[key].serialized_hash == right[key].serialized_hash for key in keys),
            "left_only", len(left.keys() - right.keys()),
            "right_only", len(right.keys() - left.keys()),
        )
        if (left_name, right_name) == ("pc-pristine", "ps2-pristine"):
            for key in sorted(keys):
                a, b = left[key], right[key]
                if core_signature(a) == core_signature(b):
                    continue
                differences = []
                for label, av, bv in (
                    ("position", a.position, b.position),
                    ("animated", a.animated, b.animated),
                    ("nodes", a.node_count, b.node_count),
                    ("transition", a.transition_values, b.transition_values),
                    ("portal_transition", a.portal_transition_values,
                     b.portal_transition_values),
                    ("links", a.links, b.links),
                    ("mesh_link_classes",
                     (a.shared_edge_links, a.manual_links, a.omitted_shared_edges),
                     (b.shared_edge_links, b.manual_links, b.omitted_shared_edges)),
                ):
                    if av != bv:
                        differences.append(label)
                print("  difference", key, differences)

    print("\nexamples (pc-pristine)")
    for item in (value for value in items if value.corpus == "pc-pristine"):
        print(
            canonical_path(item.path), f"[{item.object_index}]", item.name,
            f"nodes={item.node_count}", f"portals={len(item.portals)}",
            f"links={sum(len(x[1]) for x in item.links)}",
            f"mesh={item.mesh.encoding}:{item.mesh.target_name}",
        )

    print("\nanalysis rows")
    for label, sql in (
        ("field_definitions", "SELECT COUNT(*) FROM field_definitions WHERE type_hash=?"),
        ("variants", "SELECT COUNT(*) FROM class_variants WHERE type_hash=?"),
        ("evidence", "SELECT COUNT(*) FROM evidence WHERE type_hash=?"),
        ("annotated_fields", """
            SELECT COUNT(*) FROM direct_fields d JOIN objects o
              ON o.file_id=d.file_id AND o.object_index=d.object_index
            WHERE o.type_hash=? AND d.semantic_key IS NOT NULL
            """),
        ("variant_assignments", """
            SELECT COUNT(*) FROM object_variant_assignments a
            JOIN class_variants v ON v.id=a.variant_id WHERE v.type_hash=?
            """),
    ):
        print(label, connection.execute(sql, (MESH_NAVIGATION_SET,)).fetchone()[0])
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
