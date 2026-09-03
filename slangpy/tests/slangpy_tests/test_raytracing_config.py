# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import json
from typing import Any

import pytest

from slangpy import HitGroupDesc, RayTracingPipelineFlags
from slangpy.core.function import FunctionNode, FunctionNodeRayTracing, PipelineType
from slangpy.core.native import FunctionNodeType


def make_function_node() -> FunctionNode:
    """Create a rootless node for testing configuration without compiling a shader."""
    return FunctionNode(None, FunctionNodeType.kernelgen, None)


def signature_configuration(node: FunctionNodeRayTracing) -> dict[str, Any]:
    prefix = "ray_tracing:"
    assert node.ray_tracing_signature.startswith(prefix)
    return json.loads(node.ray_tracing_signature[len(prefix) :])


def test_legacy_ray_tracing_configuration_and_signature() -> None:
    hit_group = HitGroupDesc("primary", "closest", "any", "intersection")

    node = make_function_node().ray_tracing(
        [hit_group],
        ["miss"],
        ["primary_override"],
        ["callable"],
        2,
        48,
        12,
        RayTracingPipelineFlags.skip_triangles,
    )

    configuration = signature_configuration(node)
    assert configuration == {
        "mode": "legacy",
        "trace_program_layout": None,
        "hit_groups": [
            {
                "hit_group_name": "primary",
                "closest_hit_entry_point": "closest",
                "any_hit_entry_point": "any",
                "intersection_entry_point": "intersection",
            }
        ],
        "miss_entry_points": ["miss"],
        "hit_group_names": ["primary_override"],
        "callable_entry_points": ["callable"],
        "max_recursion": 2,
        "max_ray_payload_size": 48,
        "max_attribute_size": 12,
        "flags": int(RayTracingPipelineFlags.skip_triangles),
    }
    assert node.slangpy_signature == node.ray_tracing_signature

    info = node.calc_build_info()
    assert info.pipeline_type == PipelineType.ray_tracing
    assert info.ray_tracing_trace_program_layout is None
    assert info.ray_tracing_signature == node.ray_tracing_signature
    assert info.ray_tracing_hit_group_names == ["primary_override"]

    # The node owns a snapshot, so caller mutation cannot change its cache identity.
    hit_group.hit_group_name = "mutated"
    assert info.ray_tracing_hit_groups[0].hit_group_name == "primary"
    assert signature_configuration(node)["hit_groups"][0]["hit_group_name"] == "primary"


def test_equivalent_legacy_configurations_have_the_same_signature() -> None:
    first = make_function_node().ray_tracing(
        [HitGroupDesc("primary", "closest")],
        hit_group_names=["primary"],
    )
    second = make_function_node().ray_tracing(
        [{"hit_group_name": "primary", "closest_hit_entry_point": "closest"}],
        hit_group_names=["primary"],
    )

    assert first.ray_tracing_signature == second.ray_tracing_signature


def test_hit_group_names_affect_the_ray_tracing_signature() -> None:
    first = make_function_node().ray_tracing([], hit_group_names=["first"])
    second = make_function_node().ray_tracing([], hit_group_names=["second"])

    assert first.ray_tracing_signature != second.ray_tracing_signature


def test_structural_ray_tracing_configuration() -> None:
    first = make_function_node().ray_tracing(
        max_recursion=3,
        max_ray_payload_size=64,
        max_attribute_size=16,
        flags=RayTracingPipelineFlags.skip_procedurals,
        trace_program_layout="FirstProgramLayout",
    )
    second = make_function_node().ray_tracing(trace_program_layout="SecondProgramLayout")

    configuration = signature_configuration(first)
    assert configuration["mode"] == "structural"
    assert configuration["trace_program_layout"] == "FirstProgramLayout"
    assert configuration["hit_groups"] == []
    assert configuration["miss_entry_points"] == []
    assert configuration["hit_group_names"] is None
    assert configuration["callable_entry_points"] == []
    assert configuration["max_recursion"] == 3
    assert configuration["max_ray_payload_size"] == 64
    assert configuration["max_attribute_size"] == 16
    assert configuration["flags"] == int(RayTracingPipelineFlags.skip_procedurals)
    assert first.ray_tracing_signature != second.ray_tracing_signature

    info = first.calc_build_info()
    assert info.ray_tracing_trace_program_layout == "FirstProgramLayout"
    assert info.ray_tracing_hit_groups == []
    assert info.ray_tracing_miss_entry_points == []
    assert info.ray_tracing_callable_entry_points == []
    assert info.ray_tracing_signature == first.ray_tracing_signature


@pytest.mark.parametrize(
    "legacy_configuration, option_name",
    [
        ({"hit_groups": []}, "hit_groups"),
        ({"miss_entry_points": []}, "miss_entry_points"),
        ({"hit_group_names": []}, "hit_group_names"),
        ({"callable_entry_points": []}, "callable_entry_points"),
    ],
)
def test_structural_and_legacy_configuration_are_mutually_exclusive(
    legacy_configuration: dict[str, Any], option_name: str
) -> None:
    with pytest.raises(ValueError, match=option_name):
        make_function_node().ray_tracing(
            trace_program_layout="ProgramLayout", **legacy_configuration
        )


def test_ray_tracing_requires_one_configuration_mode() -> None:
    with pytest.raises(ValueError, match="hit_groups must be specified"):
        make_function_node().ray_tracing()

    with pytest.raises(ValueError, match="non-empty string"):
        make_function_node().ray_tracing(trace_program_layout="")
