#!/usr/bin/env python3
"""Align semantic virtual-register webs across allocator provenance captures.

Virtual-register numbers are deliberately excluded from the matching score.
The aligner instead combines PCode position and structure, instruction-creation
lineage, virtual-register creation/object evidence, lifetime shape, and graph
shape.  Its JSON report keeps low-margin assignments visibly ambiguous.
"""

import argparse
import json
import math
from collections import Counter
from pathlib import Path


FORMAT = "mwcc-semantic-register-web-alignment-v1"
MIN_MATCH_SCORE = 0.40
AMBIGUITY_MARGIN = 0.07
MIN_CONFIDENT_SCORE = 0.60
COMPONENT_WEIGHTS = {
    "pcode_position": 0.24,
    "pcode_signature": 0.15,
    "pcode_context": 0.14,
    "creation_lineage": 0.11,
    "register_origin": 0.12,
    "object_binding": 0.06,
    "lifetime_shape": 0.11,
    "graph_shape": 0.07,
}


def load_provenance(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        provenance = json.load(stream)
    if provenance.get("format") != "mwcc-allocator-provenance-v1":
        raise ValueError(f"{path}: expected mwcc-allocator-provenance-v1")
    return provenance


def multiset_dice(left: Counter, right: Counter) -> float | None:
    total = sum(left.values()) + sum(right.values())
    if total == 0:
        return None
    common = sum((left & right).values())
    return 2.0 * common / total


def categorical_similarity(left: tuple, right: tuple) -> float:
    if not left and not right:
        return 1.0
    pairs = [
        (
            left[index] if index < len(left) else None,
            right[index] if index < len(right) else None,
        )
        for index in range(max(len(left), len(right)))
    ]
    informative = [pair for pair in pairs if pair != (None, None)]
    if not informative:
        return 1.0
    return sum(a == b for a, b in informative) / len(informative)


def ratio_similarity(left: int | float, right: int | float) -> float:
    if left == right:
        return 1.0
    maximum = max(abs(left), abs(right))
    return min(abs(left), abs(right)) / maximum if maximum else 1.0


def object_signature(snapshot: dict | None) -> tuple:
    if not snapshot:
        return ()
    type_info = snapshot.get("type") or {}
    return (
        snapshot.get("object_tag_00"),
        snapshot.get("kind_02"),
        snapshot.get("flags_12"),
        type_info.get("kind_00"),
        type_info.get("size_02"),
        type_info.get("flags_0a"),
        type_info.get("subtype_0e"),
    )


def operand_role(operand: dict) -> str:
    roles = []
    if operand.get("is_definition") or operand.get("flags", 0) & 2:
        roles.append("definition")
    if operand.get("is_use") or operand.get("flags", 0) & 1:
        roles.append("use")
    if operand.get("is_last_use") or operand.get("flags", 0) & 4:
        roles.append("last_use")
    return "+".join(roles) or "other"


def stable_operand_signature(operand: dict) -> tuple:
    kind = operand.get("kind")
    register_class = operand.get("register_class")
    register = operand.get("register")
    if register_class is not None:
        value = (
            "virtual",
            register_class,
        ) if register is not None and register >= 32 else (
            "physical",
            register_class,
            register,
        )
    elif kind == 4:
        value = ("immediate", operand.get("value_signed"))
    elif kind in (2, 3):
        value = ("special", register)
    else:
        value = ("opaque",)
    return (kind, operand_role(operand), value)


def select_nodes(provenance: dict, phase: str) -> dict[str, dict]:
    nodes = provenance.get("coloring_nodes", [])
    available = [node for node in nodes if node.get("phase") == phase]
    if not available and phase == "after":
        available = [node for node in nodes if node.get("phase") == "before"]
    attempts = {}
    for node in available:
        register_id = node["register"]
        if node.get("attempt", 0) >= attempts.get(register_id, -1):
            attempts[register_id] = node.get("attempt", 0)
    return {
        node["register"]: node
        for node in available
        if node.get("attempt", 0) == attempts[node["register"]]
    }


def select_simplify_positions(provenance: dict) -> dict[str, int]:
    records = provenance.get("simplify_order", [])
    before = [record for record in records if record.get("phase") == "before"]
    chosen = before or records
    latest = {}
    for record in chosen:
        register_id = record["register"]
        attempt = record.get("attempt", 0)
        if attempt >= latest.get(register_id, (-1, -1))[0]:
            latest[register_id] = (attempt, record["position"])
    return {register_id: value[1] for register_id, value in latest.items()}


def allocation_stratum(provenance: dict, register_id: str) -> str:
    register_class, number_text = register_id.split(":", 1)
    number = int(number_text)
    if number < 32:
        return "physical"

    initial_last = None
    for boundary in provenance.get("virtual_register_boundaries", []):
        if boundary.get("phase") == "initial":
            initial_last = boundary.get("initial_object_register_last", {}).get(
                register_class
            )
            break
    if initial_last is not None and 32 <= number <= initial_last:
        return "initial_object"

    windows = [
        window
        for window in provenance.get("coalescing_windows", [])
        if window.get("register_class") == register_class
    ]
    after = [window for window in windows if window.get("phase") == "after"]
    window = (after or windows)[-1] if windows else None
    if window is None:
        return "virtual"
    if number < window["first"]:
        return "pre_coalescing_window"
    if number < window["last"]:
        return "coalescing_window"
    return "post_coalescing_window"


def build_profiles(provenance: dict, phase: str) -> dict[str, dict]:
    instructions = {item["id"]: item for item in provenance["instructions"]}
    operands = {item["id"]: item for item in provenance["operands"]}
    instruction_operands = {}
    for operand in provenance["operands"]:
        instruction_operands.setdefault(operand["instruction"], []).append(operand)
    for values in instruction_operands.values():
        values.sort(key=lambda item: item["index"])

    ordered = sorted(instructions.values(), key=lambda item: item["sequence"])
    instruction_by_sequence = {item["sequence"]: item for item in ordered}

    def instruction_signature(instruction: dict | None) -> tuple:
        if instruction is None:
            return ()
        return (
            instruction.get("opcode"),
            tuple(
                stable_operand_signature(operand)
                for operand in instruction_operands.get(instruction["id"], [])
            ),
        )

    creations = {
        item["id"]: item for item in provenance.get("pcode_creations", [])
    }
    created_by = {
        item["instruction"]: item["creation"]
        for item in provenance.get("created_by", [])
    }
    clones = {item["id"]: item for item in provenance.get("pcode_clones", [])}
    derived_from = {
        item["instruction"]: item for item in provenance.get("derived_from", [])
    }

    def lineage(instruction_id: str, seen: frozenset[str] = frozenset()) -> tuple:
        if instruction_id in seen:
            return ("cycle",)
        creation_id = created_by.get(instruction_id)
        if creation_id is not None:
            creation = creations[creation_id]
            return (
                "creation",
                creation.get("opcode"),
                creation.get("wrapper"),
                creation.get("call_address"),
                creation.get("epoch"),
            )
        relation = derived_from.get(instruction_id)
        if relation is not None:
            clone = clones.get(relation.get("clone"), {})
            return (
                "clone",
                clone.get("call_address"),
                clone.get("epoch"),
                lineage(
                    relation["source_instruction"],
                    seen | frozenset((instruction_id,)),
                ),
            )
        return ()

    virtual_creations = {
        item["id"]: item
        for item in provenance.get("virtual_register_creations", [])
    }
    register_origins = {}
    for link in provenance.get("register_created_by", []):
        event = virtual_creations.get(link["creation"])
        if event is None:
            continue
        snapshot = event.get("object_after") or event.get("object_before")
        register_origins.setdefault(link["register"], []).append(
            {
                "event": event,
                "role": link.get("role"),
                "object_signature": object_signature(snapshot),
            }
        )

    nodes = select_nodes(provenance, phase)
    simplify_positions = select_simplify_positions(provenance)
    total_instructions = max(len(ordered) - 1, 1)
    profiles = {}
    for register in provenance["registers"]:
        if not register.get("is_virtual"):
            continue
        register_id = register["id"]
        occurrence_ids = register.get("occurrences", [])
        occurrence_records = []
        for operand_id in occurrence_ids:
            operand = operands[operand_id]
            instruction = instructions[operand["instruction"]]
            sequence = instruction["sequence"]
            previous = instruction_by_sequence.get(sequence - 1)
            following = instruction_by_sequence.get(sequence + 1)
            role = operand_role(operand)
            occurrence_records.append(
                {
                    "operand": operand,
                    "instruction": instruction,
                    "role": role,
                    "position": (
                        instruction.get("block"),
                        instruction.get("block_instruction_index"),
                        instruction.get("opcode"),
                        operand["index"],
                        role,
                    ),
                    "signature": (
                        instruction.get("opcode"),
                        operand["index"],
                        role,
                        stable_operand_signature(operand),
                    ),
                    "context": (
                        instruction_signature(previous),
                        instruction_signature(instruction),
                        instruction_signature(following),
                        operand["index"],
                        role,
                    ),
                    "lineage": (
                        lineage(instruction["id"]),
                        operand["index"],
                        role,
                    ),
                }
            )

        sequences = [item["instruction"]["sequence"] for item in occurrence_records]
        block_counts = Counter(
            item["instruction"].get("block") for item in occurrence_records
        )
        origins = register_origins.get(register_id, [])
        node = nodes.get(register_id)
        profiles[register_id] = {
            "id": register_id,
            "class": register["class"],
            "number": register["register"],
            "position": Counter(item["position"] for item in occurrence_records),
            "signature": Counter(item["signature"] for item in occurrence_records),
            "context": Counter(item["context"] for item in occurrence_records),
            "lineage": Counter(
                item["lineage"]
                for item in occurrence_records
                if item["lineage"][0]
            ),
            "definition_count": len(register.get("definitions", [])),
            "use_count": len(register.get("uses", [])),
            "last_use_count": len(register.get("last_uses", [])),
            "occurrence_count": len(occurrence_records),
            "block_counts": block_counts,
            "first_position": min(sequences) / total_instructions if sequences else 0,
            "last_position": max(sequences) / total_instructions if sequences else 0,
            "span": (max(sequences) - min(sequences)) / total_instructions
            if sequences
            else 0,
            "origins": origins,
            "node": node,
            "simplify_position": simplify_positions.get(register_id),
            "allocation_stratum": allocation_stratum(provenance, register_id),
        }
    return profiles


def origin_similarity(left: dict, right: dict) -> tuple[float | None, float | None]:
    left_origins = left["origins"]
    right_origins = right["origins"]
    if not left_origins and not right_origins:
        return None, None
    if not left_origins or not right_origins:
        return 0.0, 0.0

    def event_signature(origin: dict) -> tuple:
        event = origin["event"]
        return (
            event.get("register_class"),
            event.get("allocation_kind"),
            event.get("allocator_address"),
            event.get("allocator_operation_category"),
            event.get("call_address"),
            origin.get("role"),
        )

    def best_pair_score(signature_function) -> float:
        scores = [
            categorical_similarity(signature_function(a), signature_function(b))
            for a in left_origins
            for b in right_origins
        ]
        return max(scores)

    origin_score = best_pair_score(event_signature)
    object_scores = [
        categorical_similarity(a["object_signature"], b["object_signature"])
        for a in left_origins
        for b in right_origins
        if a["object_signature"] or b["object_signature"]
    ]
    return origin_score, max(object_scores) if object_scores else None


def lifetime_similarity(left: dict, right: dict) -> float:
    count_score = sum(
        ratio_similarity(left[field], right[field])
        for field in (
            "definition_count",
            "use_count",
            "last_use_count",
            "occurrence_count",
        )
    ) / 4
    block_score = multiset_dice(left["block_counts"], right["block_counts"])
    position_score = sum(
        max(0.0, 1.0 - abs(left[field] - right[field]))
        for field in ("first_position", "last_position", "span")
    ) / 3
    return (count_score * 0.45) + ((block_score or 0.0) * 0.25) + (
        position_score * 0.30
    )


def graph_similarity(left: dict, right: dict) -> float | None:
    left_node = left["node"]
    right_node = right["node"]
    if left_node is None and right_node is None:
        return None
    if left_node is None or right_node is None:
        return 0.0

    def neighbor_shape(node: dict) -> tuple[int, int]:
        physical = sum(int(item.split(":", 1)[1]) < 32 for item in node["neighbors"])
        return physical, len(node["neighbors"]) - physical

    left_neighbors = neighbor_shape(left_node)
    right_neighbors = neighbor_shape(right_node)
    return sum(
        (
            ratio_similarity(len(left_node["neighbors"]), len(right_node["neighbors"])),
            ratio_similarity(left_neighbors[0], right_neighbors[0]),
            ratio_similarity(left_neighbors[1], right_neighbors[1]),
            1.0 if left_node.get("flags") == right_node.get("flags") else 0.0,
            ratio_similarity(
                left_node.get("spill_cost", 0), right_node.get("spill_cost", 0)
            ),
        )
    ) / 5


def score_profiles(left: dict, right: dict) -> tuple[float, dict[str, float]]:
    if left["class"] != right["class"]:
        return 0.0, {}
    origin_score, object_score = origin_similarity(left, right)
    values = {
        "pcode_position": multiset_dice(left["position"], right["position"]),
        "pcode_signature": multiset_dice(left["signature"], right["signature"]),
        "pcode_context": multiset_dice(left["context"], right["context"]),
        "creation_lineage": multiset_dice(left["lineage"], right["lineage"]),
        "register_origin": origin_score,
        "object_binding": object_score,
        "lifetime_shape": lifetime_similarity(left, right),
        "graph_shape": graph_similarity(left, right),
    }
    available = {key: value for key, value in values.items() if value is not None}
    total_weight = sum(COMPONENT_WEIGHTS[key] for key in available)
    score = sum(
        COMPONENT_WEIGHTS[key] * value for key, value in available.items()
    ) / total_weight
    return score, {key: round(value, 4) for key, value in available.items()}


def maximum_weight_assignment(weights: list[list[float]]) -> list[int | None]:
    """Return a maximum-weight row-to-column assignment (Hungarian method)."""
    if not weights:
        return []
    row_count = len(weights)
    real_column_count = len(weights[0])
    padded = [row + [MIN_MATCH_SCORE] * row_count for row in weights]
    column_count = len(padded[0])
    potentials_rows = [0.0] * (row_count + 1)
    potentials_columns = [0.0] * (column_count + 1)
    matching = [0] * (column_count + 1)
    predecessor = [0] * (column_count + 1)

    for row_index in range(1, row_count + 1):
        matching[0] = row_index
        current_column = 0
        minimum = [math.inf] * (column_count + 1)
        used = [False] * (column_count + 1)
        while True:
            used[current_column] = True
            current_row = matching[current_column]
            delta = math.inf
            next_column = 0
            for column_index in range(1, column_count + 1):
                if used[column_index]:
                    continue
                cost = (
                    1.0
                    - padded[current_row - 1][column_index - 1]
                    - potentials_rows[current_row]
                    - potentials_columns[column_index]
                )
                if cost < minimum[column_index]:
                    minimum[column_index] = cost
                    predecessor[column_index] = current_column
                if minimum[column_index] < delta:
                    delta = minimum[column_index]
                    next_column = column_index
            for column_index in range(column_count + 1):
                if used[column_index]:
                    potentials_rows[matching[column_index]] += delta
                    potentials_columns[column_index] -= delta
                else:
                    minimum[column_index] -= delta
            current_column = next_column
            if matching[current_column] == 0:
                break
        while True:
            next_column = predecessor[current_column]
            matching[current_column] = matching[next_column]
            current_column = next_column
            if current_column == 0:
                break

    assignment = [None] * row_count
    for column_index, row_index in enumerate(matching[1:], 1):
        if row_index and column_index <= real_column_count:
            assignment[row_index - 1] = column_index - 1
    return assignment


def node_state(profile: dict) -> dict:
    node = profile["node"] or {}
    coalesced = node.get("is_coalesced", False)
    return {
        "register": profile["id"],
        "physical_color": None if coalesced else node.get("color_or_parent"),
        "coalesced_parent": node.get("color_or_parent") if coalesced else None,
        "simplify_position": profile["simplify_position"],
        "allocation_stratum": profile["allocation_stratum"],
        "graph_degree": len(node.get("neighbors", [])) if node else None,
        "spill_cost": node.get("spill_cost"),
        "flags": node.get("flags"),
    }


def graph_edge_changes(
    left: dict,
    right: dict,
    confident_mapping: dict[str, str],
) -> dict:
    left_neighbors = set((left["node"] or {}).get("neighbors", []))
    right_neighbors = set((right["node"] or {}).get("neighbors", []))

    def translate(register_id: str) -> str | None:
        if int(register_id.split(":", 1)[1]) < 32:
            return register_id
        return confident_mapping.get(register_id)

    reverse = {candidate: baseline for baseline, candidate in confident_mapping.items()}
    removed = []
    unresolved_left = []
    for neighbor in sorted(left_neighbors):
        translated = translate(neighbor)
        if translated is None:
            unresolved_left.append(neighbor)
        elif translated not in right_neighbors:
            removed.append({"old": neighbor, "expected_new": translated})

    added = []
    unresolved_right = []
    for neighbor in sorted(right_neighbors):
        if int(neighbor.split(":", 1)[1]) < 32:
            baseline = neighbor
        else:
            baseline = reverse.get(neighbor)
        if baseline is None:
            unresolved_right.append(neighbor)
        elif baseline not in left_neighbors:
            added.append({"new": neighbor, "expected_old": baseline})
    return {
        "removed": removed,
        "added": added,
        "unresolved_old": unresolved_left,
        "unresolved_new": unresolved_right,
    }


def confidence_label(score: float, margin: float, reciprocal: bool) -> str:
    if score >= 0.82 and margin >= AMBIGUITY_MARGIN and reciprocal:
        return "high"
    if score >= 0.62 and margin >= AMBIGUITY_MARGIN / 2:
        return "medium"
    return "low"


def align_register_webs(
    left_provenance: dict,
    right_provenance: dict,
    register_class: str | None = None,
    phase: str = "after",
    candidate_limit: int = 3,
) -> dict:
    left_profiles = build_profiles(left_provenance, phase)
    right_profiles = build_profiles(right_provenance, phase)
    if register_class is not None:
        left_profiles = {
            key: value
            for key, value in left_profiles.items()
            if value["class"] == register_class
        }
        right_profiles = {
            key: value
            for key, value in right_profiles.items()
            if value["class"] == register_class
        }

    left_ids = sorted(
        left_profiles,
        key=lambda key: (left_profiles[key]["class"], left_profiles[key]["number"]),
    )
    right_ids = sorted(
        right_profiles,
        key=lambda key: (right_profiles[key]["class"], right_profiles[key]["number"]),
    )
    scores = []
    components = {}
    for left_id in left_ids:
        score_row = []
        for right_id in right_ids:
            score, evidence = score_profiles(
                left_profiles[left_id], right_profiles[right_id]
            )
            score_row.append(score)
            components[(left_id, right_id)] = evidence
        scores.append(score_row)

    assignment = maximum_weight_assignment(scores)
    candidate_rankings = {}
    reciprocal_best = {}
    for left_index, left_id in enumerate(left_ids):
        ranking = sorted(
            (
                (right_ids[right_index], scores[left_index][right_index])
                for right_index in range(len(right_ids))
                if left_profiles[left_id]["class"]
                == right_profiles[right_ids[right_index]]["class"]
            ),
            key=lambda item: (-item[1], item[0]),
        )
        candidate_rankings[left_id] = ranking
    for right_index, right_id in enumerate(right_ids):
        column = [
            (left_ids[left_index], scores[left_index][right_index])
            for left_index in range(len(left_ids))
            if left_profiles[left_ids[left_index]]["class"]
            == right_profiles[right_id]["class"]
        ]
        reciprocal_best[right_id] = (
            max(column, key=lambda item: item[1])[0] if column else None
        )

    provisional = []
    used_right = set()
    for left_index, left_id in enumerate(left_ids):
        right_index = assignment[left_index]
        if right_index is None or scores[left_index][right_index] < MIN_MATCH_SCORE:
            provisional.append((left_id, None, "deleted", 0.0, 0.0, False))
            continue
        right_id = right_ids[right_index]
        used_right.add(right_id)
        score = scores[left_index][right_index]
        other_scores = [
            value
            for candidate, value in candidate_rankings[left_id]
            if candidate != right_id
        ]
        margin = score - max(other_scores, default=0.0)
        reciprocal = reciprocal_best[right_id] == left_id
        assigned_is_best = candidate_rankings[left_id][0][0] == right_id
        status = (
            "matched"
            if score >= MIN_CONFIDENT_SCORE
            and margin >= AMBIGUITY_MARGIN
            and reciprocal
            and assigned_is_best
            else "ambiguous"
        )
        provisional.append((left_id, right_id, status, score, margin, reciprocal))

    confident_mapping = {
        left_id: right_id
        for left_id, right_id, status, _score, _margin, _reciprocal in provisional
        if status == "matched" and right_id is not None
    }
    mappings = []
    deleted = []
    ambiguous = []
    for left_id, right_id, status, score, margin, reciprocal in provisional:
        ranking = candidate_rankings[left_id][:candidate_limit]
        candidates = [
            {
                "register": candidate,
                "score": round(candidate_score, 4),
                "evidence": components[(left_id, candidate)],
            }
            for candidate, candidate_score in ranking
        ]
        if right_id is None:
            record = {
                "register": left_id,
                "old": node_state(left_profiles[left_id]),
                "candidates": candidates,
            }
            deleted.append(record)
            continue
        left_profile = left_profiles[left_id]
        right_profile = right_profiles[right_id]
        record = {
            "status": status,
            "old_register": left_id,
            "new_register": right_id,
            "confidence": {
                "score": round(score, 4),
                "margin": round(margin, 4),
                "label": confidence_label(score, margin, reciprocal),
                "reciprocal_best": reciprocal,
            },
            "evidence": components[(left_id, right_id)],
            "candidates": candidates,
            "old": node_state(left_profile),
            "new": node_state(right_profile),
            "changes": {
                "physical_color": {
                    "old": node_state(left_profile)["physical_color"],
                    "new": node_state(right_profile)["physical_color"],
                },
                "simplify_position": {
                    "old": left_profile["simplify_position"],
                    "new": right_profile["simplify_position"],
                },
                "allocation_stratum": {
                    "old": left_profile["allocation_stratum"],
                    "new": right_profile["allocation_stratum"],
                },
                "graph_edges": graph_edge_changes(
                    left_profile, right_profile, confident_mapping
                ),
            },
        }
        mappings.append(record)
        if status == "ambiguous":
            ambiguous.append(
                {
                    "old_register": left_id,
                    "assigned_candidate": right_id,
                    "confidence": record["confidence"],
                    "candidates": candidates,
                }
            )

    inserted = [
        {
            "register": right_id,
            "new": node_state(right_profiles[right_id]),
        }
        for right_id in right_ids
        if right_id not in used_right
    ]
    mappings.sort(
        key=lambda item: (
            item["old_register"].split(":", 1)[0],
            int(item["old_register"].split(":", 1)[1]),
        )
    )
    return {
        "format": FORMAT,
        "left": {
            "capture_index": left_provenance.get("capture_index"),
            "function_pointer": left_provenance.get("function_pointer"),
        },
        "right": {
            "capture_index": right_provenance.get("capture_index"),
            "function_pointer": right_provenance.get("function_pointer"),
        },
        "register_class": register_class or "all",
        "coloring_phase": phase,
        "summary": {
            "matched": sum(item["status"] == "matched" for item in mappings),
            "ambiguous": len(ambiguous),
            "inserted": len(inserted),
            "deleted": len(deleted),
        },
        "mappings": mappings,
        "ambiguous": ambiguous,
        "inserted": inserted,
        "deleted": deleted,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Align semantic virtual-register webs across provenance captures"
    )
    parser.add_argument("left", type=Path, help="baseline provenance JSON")
    parser.add_argument("right", type=Path, help="candidate provenance JSON")
    parser.add_argument("--register-class", choices=("gpr", "fpr", "vr"))
    parser.add_argument("--phase", default="after", choices=("before", "after"))
    parser.add_argument("--candidate-limit", type=int, default=3)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.candidate_limit < 1:
        parser.error("--candidate-limit must be positive")

    result = align_register_webs(
        load_provenance(args.left),
        load_provenance(args.right),
        args.register_class,
        args.phase,
        args.candidate_limit,
    )
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
