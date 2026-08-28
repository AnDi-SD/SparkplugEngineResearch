#!/usr/bin/env python3
"""Read-only corpus report for spBSPNode in schema v2."""

from __future__ import annotations

import argparse
import collections
import json
import math
import sqlite3
import struct
from dataclasses import dataclass
from pathlib import Path


BSP_NODE = 0x7362AB22
PARTITION_SYSTEM = 0x912CC341
PARTITION_NODE = 0x67672341
STATIC_RENDER_OBJECT = 0x56D67170
COLLISION_INFO = 0x47A97C0E
ZONE_PORTAL = 0x6523AC37


@dataclass(frozen=True)
class Child:
    slot: int
    object_id: int
    inline_size: int
    target_index: int | None
    target_type: int | None


@dataclass(frozen=True)
class Node:
    corpus: str
    file_id: int
    path: str
    index: int
    object_id: int
    parent_index: int | None
    parent_type: int | None
    serialized_size: int
    debug_color: int
    partition_system_id: int
    zone_id: int
    children: tuple[Child, ...]
    partition_renderable_id: int
    plane: tuple[float, float, float, float]
    polygon: tuple[tuple[float, float, float], ...]


def relationship_prefix(payload: bytes, payload_size: int) -> tuple[int, int, str]:
    if payload_size == 4:
        return struct.unpack_from("<I", payload)[0], 0, "id_only"
    if payload_size < 8 or len(payload) < 8:
        raise ValueError(f"invalid relationship size {payload_size}")
    object_id, inline_size = struct.unpack_from("<II", payload)
    if inline_size != payload_size - 8:
        raise ValueError(
            f"inline size {inline_size} does not match payload {payload_size}"
        )
    return object_id, inline_size, "inline" if inline_size else "sized_reference"


def normalized_path(path: str) -> str:
    value = path.replace("\\", "/").lower()
    if value.startswith("data/"):
        value = value[5:]
    return value


def load_nodes(connection: sqlite3.Connection) -> list[Node]:
    object_rows = connection.execute(
        """
        SELECT c.corpus_key,o.file_id,f.relative_path,o.object_index,o.object_id,
               o.parent_index,p.type_hash,o.serialized_size
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        LEFT JOIN objects p ON p.file_id=o.file_id AND p.object_index=o.parent_index
        WHERE o.type_hash=? ORDER BY c.id,o.file_id,o.object_index
        """,
        (BSP_NODE,),
    ).fetchall()
    objects_by_id = {
        (file_id, object_id): (index, type_hash)
        for file_id, index, object_id, type_hash in connection.execute(
            "SELECT file_id,object_index,object_id,type_hash FROM objects"
        )
    }
    result: list[Node] = []
    for corpus, file_id, path, index, object_id, parent_index, parent_type, size in object_rows:
        fields = connection.execute(
            """
            SELECT section_index,field_type,payload_size,payload_preview
            FROM direct_fields WHERE file_id=? AND object_index=?
              AND is_section_terminator=0 ORDER BY field_index
            """,
            (file_id, index),
        ).fetchall()
        debug_color = partition_system_id = zone_id = partition_renderable_id = -1
        children: list[Child] = []
        plane: tuple[float, float, float, float] | None = None
        polygon: tuple[tuple[float, float, float], ...] = ()
        for section, field_type, payload_size, preview in fields:
            payload = bytes(preview)
            if section == 0 and field_type == 1:
                debug_color = struct.unpack_from("<I", payload)[0]
            elif section == 0 and field_type == 5:
                partition_system_id = relationship_prefix(payload, payload_size)[0]
            elif section == 0 and field_type == 3:
                zone_id = relationship_prefix(payload, payload_size)[0]
            elif section == 0 and field_type == 2:
                slot = struct.unpack_from("<I", payload)[0]
                child_id, inline_size, _ = relationship_prefix(payload[4:], payload_size - 4)
                target = objects_by_id.get((file_id, child_id))
                children.append(
                    Child(
                        slot,
                        child_id,
                        inline_size,
                        target[0] if target else None,
                        target[1] if target else None,
                    )
                )
            elif section == 0 and field_type == 6:
                partition_renderable_id = relationship_prefix(payload, payload_size)[0]
            elif section == 1 and field_type == 0:
                plane = struct.unpack_from("<4f", payload)
            elif section == 1 and field_type == 1:
                count = struct.unpack_from("<I", payload)[0]
                required = 4 + count * 12
                if len(payload) < required:
                    decoded = connection.execute(
                        """SELECT decoded_value FROM direct_fields
                           WHERE file_id=? AND object_index=? AND section_index=1
                             AND field_type=1 AND is_section_terminator=0""",
                        (file_id, index),
                    ).fetchone()[0]
                    if not decoded:
                        raise ValueError("polygon is larger than preview and not decoded")
                    polygon = tuple(tuple(item) for item in json.loads(decoded)["Vertices"])
                else:
                    polygon = tuple(
                        struct.unpack_from("<3f", payload, 4 + item * 12)
                        for item in range(count)
                    )
        if (
            debug_color < 0
            or partition_system_id < 0
            or zone_id < 0
            or partition_renderable_id < 0
            or plane is None
        ):
            raise ValueError(f"incomplete spBSPNode {corpus}:{path} [{index}]")
        result.append(
            Node(
                corpus,
                file_id,
                path,
                index,
                object_id,
                parent_index,
                parent_type,
                size,
                debug_color,
                partition_system_id,
                zone_id,
                tuple(children),
                partition_renderable_id,
                plane,
                polygon,
            )
        )
    return result


def axis_name(plane: tuple[float, float, float, float]) -> str:
    normal = plane[:3]
    length = math.sqrt(sum(value * value for value in normal))
    if length <= 1e-8:
        return "zero"
    unit = tuple(value / length for value in normal)
    axes = (("+X", (1.0, 0.0, 0.0)), ("-X", (-1.0, 0.0, 0.0)),
            ("+Y", (0.0, 1.0, 0.0)), ("-Y", (0.0, -1.0, 0.0)),
            ("+Z", (0.0, 0.0, 1.0)), ("-Z", (0.0, 0.0, -1.0)))
    for name, axis in axes:
        if max(abs(unit[i] - axis[i]) for i in range(3)) <= 1e-5:
            return name
    return "oblique"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("database", type=Path)
    args = parser.parse_args()
    connection = sqlite3.connect(f"file:{args.database.resolve()}?mode=ro", uri=True)
    nodes = load_nodes(connection)

    print("profiles")
    for row in connection.execute(
        """
        SELECT c.corpus_key,p.platform_key,COUNT(*),COUNT(DISTINCT o.file_id),
               MIN(o.serialized_size),MAX(o.serialized_size),SUM(o.name <> '')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id JOIN platforms p ON p.id=c.platform_id
        WHERE o.type_hash=? GROUP BY c.id ORDER BY c.id
        """,
        (BSP_NODE,),
    ):
        print(row)

    print("\nfield-shape summary")
    for row in connection.execute(
        """
        SELECT c.corpus_key,COUNT(DISTINCT o.field_shape),
               SUM(o.field_shape LIKE '%s1:f0:16|s1:end'),
               SUM(o.field_shape LIKE '%s1:f1:%')
        FROM objects o JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id WHERE o.type_hash=?
        GROUP BY c.id ORDER BY c.id
        """,
        (BSP_NODE,),
    ):
        print(row)

    print("\nbase field sizes")
    for row in connection.execute(
        """
        SELECT c.corpus_key,d.section_index,d.field_type,d.payload_size,COUNT(*)
        FROM direct_fields d JOIN objects o ON o.file_id=d.file_id
          AND o.object_index=d.object_index JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.is_section_terminator=0
        GROUP BY c.id,d.section_index,d.field_type,d.payload_size
        ORDER BY c.id,d.section_index,d.field_type,d.payload_size
        """,
        (BSP_NODE,),
    ):
        print(row)

    by_corpus: dict[str, list[Node]] = collections.defaultdict(list)
    for node in nodes:
        by_corpus[node.corpus].append(node)
    print("\nvalues and topology")
    for corpus, corpus_nodes in sorted(by_corpus.items()):
        colors = collections.Counter(f"0x{node.debug_color:08X}" for node in corpus_nodes)
        axes = collections.Counter(axis_name(node.plane) for node in corpus_nodes)
        masks = collections.Counter(
            "".join(
                "B" if child.target_type == BSP_NODE else
                "P" if child.target_type == PARTITION_NODE else "?"
                for child in node.children
            )
            for node in corpus_nodes
        )
        slots = collections.Counter(
            child.slot for node in corpus_nodes for child in node.children
        )
        zone_counts = collections.Counter(
            (node.file_id, node.zone_id) for node in corpus_nodes
        )
        lengths = [math.sqrt(sum(value * value for value in node.plane[:3])) for node in corpus_nodes]
        print(
            corpus,
            "colors", dict(colors),
            "axes", dict(axes),
            "child_masks", dict(masks),
            "slots", dict(slots),
            "zones", len(zone_counts),
            "zone_multiplicity", dict(collections.Counter(zone_counts.values())),
            "normal_length", (min(lengths), max(lengths)),
            "polygons", sum(bool(node.polygon) for node in corpus_nodes),
            "renderables", sum(bool(node.partition_renderable_id) for node in corpus_nodes),
        )

    print("\ntrees")
    grouped: dict[tuple[str, int], list[Node]] = collections.defaultdict(list)
    for node in nodes:
        grouped[(node.corpus, node.file_id)].append(node)
    tree_signatures: dict[tuple[str, str], tuple[object, ...]] = {}
    for (corpus, _file_id), tree_nodes in sorted(grouped.items()):
        lookup = {node.index: node for node in tree_nodes}
        roots = [node for node in tree_nodes if node.parent_type == PARTITION_SYSTEM]
        if len(roots) != 1:
            raise ValueError(f"expected one BSP root in {corpus}:{tree_nodes[0].path}")
        seen: set[int] = set()
        paths: dict[int, str] = {}

        def visit(node: Node, path: str, depth: int) -> int:
            if node.index in seen:
                raise ValueError(f"cycle/shared BSP node at {node.path} [{node.index}]")
            seen.add(node.index)
            paths[node.index] = path
            maximum = depth
            for child in node.children:
                if not child.object_id:
                    continue
                if child.target_type != BSP_NODE:
                    if child.target_type != PARTITION_NODE or child.inline_size != 0:
                        raise ValueError(
                            f"unexpected BSP terminal at {node.path} [{node.index}]"
                        )
                    continue
                if child.target_index not in lookup:
                    raise ValueError(f"BSP child escapes tree at {node.path} [{node.index}]")
                target = lookup[child.target_index]
                if target.parent_index != node.index or target.serialized_size != child.inline_size:
                    raise ValueError(f"BSP child ownership/size mismatch at {node.path}")
                maximum = max(maximum, visit(target, path + str(child.slot), depth + 1))
            return maximum

        depth = visit(roots[0], "", 0)
        if len(seen) != len(tree_nodes):
            raise ValueError(f"unreachable BSP node in {tree_nodes[0].path}")
        ordered = sorted(tree_nodes, key=lambda node: paths[node.index])
        signature = tuple(
            (
                paths[node.index],
                node.zone_id,
                tuple(round(value, 7) for value in node.plane),
                tuple(child.target_type for child in node.children),
            )
            for node in ordered
        )
        tree_signatures[(corpus, normalized_path(tree_nodes[0].path))] = signature
        print(
            corpus,
            tree_nodes[0].path,
            "nodes", len(tree_nodes),
            "terminal_refs", sum(
                child.target_type != BSP_NODE
                for node in tree_nodes for child in node.children
            ),
            "max_depth", depth,
            "root_size", roots[0].serialized_size,
        )

    print("\ncross-corpus tree semantics")
    paths = sorted({path for _corpus, path in tree_signatures})
    for path in paths:
        pc = tree_signatures.get(("pc-pristine", path))
        ps2 = tree_signatures.get(("ps2-pristine", path))
        if pc is not None and ps2 is not None:
            # Object IDs can differ without changing topology, so compare a normalized form.
            normalized_pc = tuple((item[0], item[2], item[3]) for item in pc)
            normalized_ps2 = tuple((item[0], item[2], item[3]) for item in ps2)
            print(path, "same_topology_and_planes", normalized_pc == normalized_ps2)

    print("\nchild-slot plane-side samples")
    # PC/PS2 topology and planes are identical; one pristine corpus is enough
    # for the more expensive content-side diagnostic below.
    for corpus in ("pc-pristine",):
        corpus_nodes = by_corpus[corpus]
        if not corpus_nodes:
            continue
        object_lookup = {
            (file_id, object_id): (index, type_hash)
            for file_id, index, object_id, type_hash in connection.execute(
                """SELECT o.file_id,o.object_index,o.object_id,o.type_hash
                   FROM objects o JOIN files f ON f.id=o.file_id
                   JOIN corpora c ON c.id=f.corpus_id WHERE c.corpus_key=?""",
                (corpus,),
            )
        }
        fields_by_object: dict[
            tuple[int, int], list[tuple[int, int, int, bytes, str | None]]
        ] = collections.defaultdict(list)
        for (
            file_id,
            object_index,
            section,
            field_type,
            payload_size,
            preview,
            decoded,
        ) in connection.execute(
            """SELECT d.file_id,d.object_index,d.section_index,d.field_type,
                      d.payload_size,d.payload_preview,d.decoded_value
               FROM direct_fields d JOIN objects o ON o.file_id=d.file_id
                 AND o.object_index=d.object_index JOIN files f ON f.id=o.file_id
               JOIN corpora c ON c.id=f.corpus_id
               WHERE c.corpus_key=? AND d.is_section_terminator=0
                 AND o.type_hash IN (?,?,?,?)""",
            (
                corpus,
                PARTITION_NODE,
                STATIC_RENDER_OBJECT,
                COLLISION_INFO,
                ZONE_PORTAL,
            ),
        ):
            fields_by_object[(file_id, object_index)].append(
                (section, field_type, payload_size, bytes(preview), decoded)
            )

        def targets(file_id: int, index: int, field_type: int) -> list[tuple[int, int]]:
            result: list[tuple[int, int]] = []
            for section, kind, payload_size, preview, _decoded in fields_by_object[
                (file_id, index)
            ]:
                if section != 0 or kind != field_type:
                    continue
                object_id = relationship_prefix(preview, payload_size)[0]
                target = object_lookup.get((file_id, object_id))
                if target:
                    result.append(target)
            return result

        point_cache: dict[tuple[int, int], tuple[tuple[float, float, float], ...]] = {}

        def leaf_points(file_id: int, index: int) -> tuple[tuple[float, float, float], ...]:
            key = (file_id, index)
            if key in point_cache:
                return point_cache[key]
            points: list[tuple[float, float, float]] = []
            for target_index, target_type in targets(file_id, index, 7):
                if target_type != STATIC_RENDER_OBJECT:
                    continue
                row = next(
                    (
                        item
                        for item in fields_by_object[(file_id, target_index)]
                        if item[0] == 0 and item[1] == 1 and item[2] == 64
                    ),
                    None,
                )
                if row and len(row[3]) >= 60:
                    points.append(struct.unpack_from("<3f", row[3], 48))
            for target_index, target_type in targets(file_id, index, 0):
                if target_type != COLLISION_INFO:
                    continue
                row = next(
                    (
                        item
                        for item in fields_by_object[(file_id, target_index)]
                        if item[0] == 0 and item[1] == 2 and item[2] == 40
                    ),
                    None,
                )
                if row and len(row[3]) >= 12:
                    points.append(struct.unpack_from("<3f", row[3], 0))
            for target_index, target_type in targets(file_id, index, 4):
                if target_type != ZONE_PORTAL:
                    continue
                row = next(
                    (
                        item
                        for item in fields_by_object[(file_id, target_index)]
                        if item[0] == 0 and item[1] == 1
                    ),
                    None,
                )
                if not row:
                    continue
                _section, _kind, payload_size, payload, decoded = row
                count = struct.unpack_from("<I", payload)[0]
                if len(payload) >= 4 + count * 12:
                    points.extend(
                        struct.unpack_from("<3f", payload, 4 + item * 12)
                        for item in range(count)
                    )
                elif decoded:
                    points.extend(tuple(item) for item in json.loads(decoded)["Vertices"])
            point_cache[key] = tuple(points)
            return point_cache[key]

        bsp_lookup = {(node.file_id, node.index): node for node in corpus_nodes}
        subtree_cache: dict[tuple[int, int], tuple[tuple[float, float, float], ...]] = {}

        def subtree_points(file_id: int, child: Child) -> tuple[tuple[float, float, float], ...]:
            if child.target_index is None:
                return ()
            key = (file_id, child.target_index)
            if child.target_type == PARTITION_NODE:
                return leaf_points(*key)
            if key in subtree_cache:
                return subtree_cache[key]
            target = bsp_lookup[key]
            value = tuple(
                point
                for grandchild in target.children
                for point in subtree_points(file_id, grandchild)
            )
            subtree_cache[key] = value
            return value

        distances: dict[int, list[float]] = collections.defaultdict(list)
        alternate_distances: dict[int, list[float]] = collections.defaultdict(list)
        empty: collections.Counter[int] = collections.Counter()
        for node in corpus_nodes:
            nx, ny, nz, constant = node.plane
            for child in node.children:
                points = subtree_points(node.file_id, child)
                if not points:
                    empty[child.slot] += 1
                distances[child.slot].extend(
                    nx * point[0] + ny * point[1] + nz * point[2] + constant
                    for point in points
                )
                alternate_distances[child.slot].extend(
                    nx * point[0] + ny * point[1] + nz * point[2] - constant
                    for point in points
                )
        for slot in sorted(distances):
            values = distances[slot]
            print(
                corpus,
                "slot", slot,
                "samples", len(values),
                "negative", sum(value < -1e-3 for value in values),
                "on", sum(abs(value) <= 1e-3 for value in values),
                "positive", sum(value > 1e-3 for value in values),
                "range", (min(values), max(values)),
                "empty_subtrees", empty[slot],
            )
            alternate = alternate_distances[slot]
            print(
                corpus,
                "slot", slot,
                "dot-minus-constant",
                "negative", sum(value < -1e-3 for value in alternate),
                "on", sum(abs(value) <= 1e-3 for value in alternate),
                "positive", sum(value > 1e-3 for value in alternate),
                "range", (min(alternate), max(alternate)),
            )

    print("\nanalyzed annotations")
    for row in connection.execute(
        """
        SELECT c.corpus_key,d.semantic_key,COUNT(*),SUM(d.is_decoded)
        FROM direct_fields d JOIN objects o ON o.file_id=d.file_id
          AND o.object_index=d.object_index JOIN files f ON f.id=o.file_id
        JOIN corpora c ON c.id=f.corpus_id
        WHERE o.type_hash=? AND d.is_section_terminator=0
        GROUP BY c.id,d.semantic_key ORDER BY c.id,d.semantic_key
        """,
        (BSP_NODE,),
    ):
        print(row)
    print("\nanalysis rows")
    for label, sql in (
        ("field_definitions", "SELECT COUNT(*) FROM field_definitions WHERE type_hash=?"),
        ("variants", "SELECT COUNT(*) FROM class_variants WHERE type_hash=?"),
        ("evidence", "SELECT COUNT(*) FROM evidence WHERE type_hash=?"),
    ):
        print(label, connection.execute(sql, (BSP_NODE,)).fetchone()[0])
    print(
        "assignments",
        connection.execute(
            """SELECT COUNT(*) FROM object_variant_assignments a
               JOIN class_variants v ON v.id=a.variant_id WHERE v.type_hash=?""",
            (BSP_NODE,),
        ).fetchone()[0],
    )
    for row in connection.execute(
        """SELECT evidence_kind,locator,observation FROM evidence
           WHERE type_hash=? ORDER BY id""",
        (BSP_NODE,),
    ):
        print(row)
    connection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
