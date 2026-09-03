// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "testing.h"

#include "sgl/device/device.h"
#include "sgl/device/raytracing.h"
#include "sgl/device/reflection.h"
#include "sgl/device/shader.h"

#include <map>

using namespace sgl;

namespace {

constexpr std::string_view k_structural_source = R"SLANG(
import slang.raytracing;

struct Payload
{
    uint value;
}

struct NonEmptyRecord
{
    uint value;
}

struct TraceContext : rt::ITraceContext
{
    typealias Payload = ::Payload;
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}

struct HitContext : rt::IHitContext
{
    typealias TraceContext = ::TraceContext;
    typealias Primitive = rt::TrianglePrimitive;
    typealias Record = void;
}

struct ClosestHit0 : rt::IClosestHitShader<HitContext>
{
    void invoke(rt::ClosestHitInput<HitContext> input) {}
}

struct ClosestHit3 : rt::IClosestHitShader<HitContext>
{
    void invoke(rt::ClosestHitInput<HitContext> input) {}
}

struct AnyHit3 : rt::IAnyHitShader<HitContext>
{
    void invoke(rt::AnyHitInput<HitContext> input) {}
}

struct HitGroup3 : rt::IHitGroup
{
    typealias Slot = rt::HitGroupSlot<3>;
    typealias Context = HitContext;
    typealias ClosestHit = ClosestHit3;
    typealias AnyHit = AnyHit3;
    typealias Intersection = rt::NoIntersection<HitContext>;
}

struct HitGroup0 : rt::IHitGroup
{
    typealias Slot = rt::HitGroupSlot<0>;
    typealias Context = HitContext;
    typealias ClosestHit = ClosestHit0;
    typealias AnyHit = rt::NoAnyHit<HitContext>;
    typealias Intersection = rt::NoIntersection<HitContext>;
}

struct MissContext : rt::IMissGroupContext
{
    typealias TraceContext = ::TraceContext;
    typealias Record = void;
}

struct Miss0 : rt::IMissShader<MissContext>
{
    void invoke(rt::MissInput<MissContext> input) {}
}

struct Miss2 : rt::IMissShader<MissContext>
{
    void invoke(rt::MissInput<MissContext> input) {}
}

struct MissGroup2 : rt::IMissGroup
{
    typealias Slot = rt::MissSlot<2>;
    typealias Context = MissContext;
    typealias Miss = Miss2;
}

struct MissGroup0 : rt::IMissGroup
{
    typealias Slot = rt::MissSlot<0>;
    typealias Context = MissContext;
    typealias Miss = Miss0;
}

struct CallableContext : rt::ICallableGroupContext
{
    typealias TraceContext = ::TraceContext;
    typealias CallableData = uint;
    typealias Record = void;
}

struct Callable4 : rt::ICallableShader<CallableContext>
{
    void invoke(rt::CallableInput<CallableContext> input) {}
}

struct CallableGroup4 : rt::ICallableGroup
{
    typealias Slot = rt::CallableSlot<4>;
    typealias Context = CallableContext;
    typealias Callable = Callable4;
}

struct SparseLayout : rt::ITraceProgramLayout
{
    typealias TraceContext = ::TraceContext;
    typealias HitGroups = rt::HitGroupList<::TraceContext, HitGroup3, HitGroup0>;
    typealias MissGroups = rt::MissGroupList<::TraceContext, MissGroup2, MissGroup0>;
    typealias CallableGroups = rt::CallableGroupList<::TraceContext, CallableGroup4>;
}

struct EmptyLayout : rt::ITraceProgramLayout
{
    typealias TraceContext = ::TraceContext;
    typealias HitGroups = rt::NoHitGroups<::TraceContext>;
    typealias MissGroups = rt::NoMissGroups<::TraceContext>;
    typealias CallableGroups = rt::NoCallableGroups<::TraceContext>;
}

namespace NamespacedStages
{
struct Miss : rt::IMissShader<::MissContext>
{
    void invoke(rt::MissInput<::MissContext> input) {}
}

struct MissGroup : rt::IMissGroup
{
    typealias Slot = rt::MissSlot<0>;
    typealias Context = ::MissContext;
    typealias Miss = NamespacedStages::Miss;
}
}

struct NamespacedLayout : rt::ITraceProgramLayout
{
    typealias TraceContext = ::TraceContext;
    typealias HitGroups = rt::NoHitGroups<::TraceContext>;
    typealias MissGroups = rt::MissGroupList<::TraceContext, NamespacedStages::MissGroup>;
    typealias CallableGroups = rt::NoCallableGroups<::TraceContext>;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void explicit_compute() {}
)SLANG";

constexpr std::string_view k_ambiguous_stage_source = R"SLANG(
import slang.raytracing;

struct DuplicatePayload
{
    uint value;
}

struct DuplicateTraceContext : rt::ITraceContext
{
    typealias Payload = DuplicatePayload;
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}

struct DuplicateMissContext : rt::IMissGroupContext
{
    typealias TraceContext = DuplicateTraceContext;
    typealias Record = void;
}

struct DuplicateMiss : rt::IMissShader<DuplicateMissContext>
{
    void invoke(rt::MissInput<DuplicateMissContext> input) {}
}
)SLANG";

ref<sgl::SlangSession> create_structural_session(Device* device)
{
    SlangCompilerOptions options = device->desc().compiler_options;
    options.enable_experimental_features = true;
    return device->create_slang_session({
        .compiler_options = std::move(options),
        .add_default_include_paths = true,
    });
}

ref<TraceProgramLayoutInfo> clone_layout_info(const TraceProgramLayoutInfo* source)
{
    auto result = make_ref<TraceProgramLayoutInfo>();
    result->source_layout = source->source_layout;
    result->type = source->type;
    result->type_name = source->type_name;
    result->trace_context_type = source->trace_context_type;
    result->hit_groups = source->hit_groups;
    result->miss_groups = source->miss_groups;
    result->callable_groups = source->callable_groups;
    return result;
}

} // namespace

TEST_SUITE_BEGIN("device");

TEST_CASE_GPU("structural ray tracing native bridge")
{
    ref<sgl::SlangSession> session = create_structural_session(ctx.device);

    SUBCASE("reflection snapshot")
    {
        ref<SlangModule> module = session->load_module_from_source("structural_reflection_test", k_structural_source);
        ref<const ProgramLayout> program_layout = module->layout();
        ref<const TraceProgramLayoutInfo> layout = program_layout->find_trace_program_layout("SparseLayout");

        REQUIRE(layout);
        CHECK(layout->is_valid());
        CHECK_EQ(layout->type_name, "SparseLayout");
        REQUIRE(layout->trace_context_type);
        CHECK_EQ(std::string(layout->trace_context_type->name()), "TraceContext");
        REQUIRE_EQ(layout->hit_groups.size(), 2);
        CHECK_EQ(layout->hit_groups[0].slot, 3);
        CHECK_EQ(layout->hit_groups[1].slot, 0);
        REQUIRE(layout->hit_groups[0].closest_hit);
        CHECK_EQ(layout->hit_groups[0].closest_hit->entry_point_name, "ClosestHit3");
        REQUIRE(layout->hit_groups[0].any_hit);
        CHECK_EQ(layout->hit_groups[0].any_hit->stage, ShaderStage::any_hit);
        CHECK_FALSE(layout->hit_groups[0].intersection);
        CHECK_FALSE(layout->hit_groups[1].any_hit);
        REQUIRE_EQ(layout->miss_groups.size(), 2);
        CHECK_EQ(layout->miss_groups[0].slot, 2);
        CHECK_EQ(layout->miss_groups[1].slot, 0);
        REQUIRE_EQ(layout->callable_groups.size(), 1);
        CHECK_EQ(layout->callable_groups[0].slot, 4);
        REQUIRE(layout->callable_groups[0].callable);
        CHECK_EQ(layout->callable_groups[0].callable->stage, ShaderStage::callable);
        CHECK_FALSE(program_layout->find_trace_program_layout("TraceContext"));

        program_layout.reset();
        module.reset();
        CHECK(layout->is_valid());
        CHECK_EQ(layout->hit_groups[1].type_name, "HitGroup0");
    }

    SUBCASE("checked entry point lookup")
    {
        ref<SlangModule> module = session->load_module_from_source("structural_entry_point_test", k_structural_source);

        ref<sgl::SlangEntryPoint> closest_hit = module->checked_entry_point("ClosestHit0", ShaderStage::closest_hit);
        REQUIRE(closest_hit);
        CHECK_EQ(closest_hit->stage(), ShaderStage::closest_hit);
        CHECK_EQ(closest_hit->name(), "ClosestHit0");

        ref<sgl::SlangEntryPoint> renamed_closest_hit = closest_hit->with_name("RenamedClosestHit0");
        REQUIRE(renamed_closest_hit);
        CHECK_EQ(renamed_closest_hit->name(), "RenamedClosestHit0");
        CHECK_EQ(renamed_closest_hit->stage(), ShaderStage::closest_hit);
        CHECK_EQ(renamed_closest_hit->desc().name, "ClosestHit0");
        REQUIRE(renamed_closest_hit->desc().requested_stage);
        CHECK_EQ(*renamed_closest_hit->desc().requested_stage, ShaderStage::closest_hit);

        ref<sgl::SlangEntryPoint> renamed_again = renamed_closest_hit->with_name("RenamedAgainClosestHit0");
        REQUIRE(renamed_again);
        CHECK_EQ(renamed_again->name(), "RenamedAgainClosestHit0");
        CHECK_EQ(renamed_again->desc().name, "ClosestHit0");

        ref<sgl::SlangEntryPoint> rebuilt_closest_hit
            = renamed_closest_hit->specialize(std::span<const SpecializationArg>{});
        REQUIRE(rebuilt_closest_hit);
        CHECK_EQ(rebuilt_closest_hit->name(), "RenamedClosestHit0");
        CHECK_EQ(rebuilt_closest_hit->stage(), ShaderStage::closest_hit);

        ref<sgl::SlangEntryPoint> legacy = module->entry_point("explicit_compute");
        REQUIRE(legacy);
        CHECK_EQ(legacy->stage(), ShaderStage::compute);

        ref<sgl::SlangEntryPoint> renamed_legacy = legacy->with_name("renamed_explicit_compute");
        REQUIRE(renamed_legacy);
        CHECK_EQ(renamed_legacy->name(), "renamed_explicit_compute");
        CHECK_EQ(renamed_legacy->stage(), ShaderStage::compute);
        CHECK_EQ(renamed_legacy->desc().name, "explicit_compute");

        CHECK_THROWS_WITH_AS(
            module->checked_entry_point("ClosestHit0", ShaderStage::miss),
            doctest::Contains("stage mismatch"),
            std::runtime_error
        );
    }

    SUBCASE("composed entry point resolution")
    {
        ref<SlangModule> module
            = session->load_module_from_source("structural_composed_stage_test", k_structural_source);
        ref<SlangModule> support = session->load_module_from_source(
            "structural_composed_support_test",
            "export uint structural_support_value() { return 1; }"
        );
        ref<SlangModule> composed = session->compose_modules("structural_composed_test", {support, module});
        ref<SlangModule> nested = session->compose_modules("structural_nested_composed_test", {support, composed});

        ref<sgl::SlangEntryPoint> entry_point = nested->checked_entry_point("Miss2", ShaderStage::miss);
        REQUIRE(entry_point);
        CHECK_EQ(entry_point->module(), module.get());
        CHECK_EQ(entry_point->stage(), ShaderStage::miss);

        ref<SlangModule> duplicate_a
            = session->load_module_from_source("structural_duplicate_a", k_ambiguous_stage_source);
        ref<SlangModule> duplicate_b
            = session->load_module_from_source("structural_duplicate_b", k_ambiguous_stage_source);
        ref<SlangModule> ambiguous = session->compose_modules("structural_ambiguous_test", {duplicate_a, duplicate_b});
        CHECK_THROWS_WITH_AS(
            ambiguous->checked_entry_point("DuplicateMiss", ShaderStage::miss),
            doctest::Contains("ambiguous across composed source modules"),
            std::runtime_error
        );
    }

    SUBCASE("sparse descriptor adapter")
    {
        ref<SlangModule> module
            = session->load_module_from_source("structural_sparse_adapter_test", k_structural_source);
        StructuralRayTracingBindings bindings = create_structural_ray_tracing_bindings(
            module.get(),
            "SparseLayout",
            {
                .min_hit_group_count = 6,
                .min_miss_count = 5,
                .min_callable_count = 7,
            }
        );

        REQUIRE_EQ(bindings.entry_points.size(), 6);
        ref<const TraceProgramLayoutInfo> sparse_layout = module->layout()->find_trace_program_layout("SparseLayout");
        REQUIRE(sparse_layout);
        std::map<std::string, std::string> expected_export_names;
        auto add_expected_stage = [&](const std::optional<TraceProgramStageInfo>& stage)
        {
            if (stage)
                expected_export_names.emplace(stage->type_name, stage->entry_point_name);
        };
        for (const auto& group : sparse_layout->hit_groups) {
            add_expected_stage(group.closest_hit);
            add_expected_stage(group.any_hit);
            add_expected_stage(group.intersection);
        }
        for (const auto& group : sparse_layout->miss_groups)
            add_expected_stage(group.miss);
        for (const auto& group : sparse_layout->callable_groups)
            add_expected_stage(group.callable);

        std::map<std::string, std::string> exported_names;
        for (const auto& entry_point : bindings.entry_points) {
            REQUIRE(entry_point->desc().export_name.has_value());
            CHECK_EQ(*entry_point->desc().export_name, entry_point->name());
            REQUIRE(expected_export_names.contains(entry_point->desc().name));
            CHECK_EQ(entry_point->name(), expected_export_names.at(entry_point->desc().name));
            CHECK(exported_names.emplace(entry_point->desc().name, entry_point->name()).second);
        }

        REQUIRE_EQ(bindings.hit_groups.size(), 2);
        CHECK_EQ(bindings.hit_groups[0].hit_group_name, "__sgl_structural_hit_group_0");
        CHECK_EQ(bindings.hit_groups[0].closest_hit_entry_point, exported_names.at("ClosestHit0"));
        CHECK(bindings.hit_groups[0].any_hit_entry_point.empty());
        CHECK_EQ(bindings.hit_groups[1].hit_group_name, "__sgl_structural_hit_group_3");
        CHECK_EQ(bindings.hit_groups[1].closest_hit_entry_point, exported_names.at("ClosestHit3"));
        CHECK_EQ(bindings.hit_groups[1].any_hit_entry_point, exported_names.at("AnyHit3"));

        REQUIRE_EQ(bindings.hit_group_names.size(), 6);
        CHECK_EQ(bindings.hit_group_names[0], "__sgl_structural_hit_group_0");
        CHECK(bindings.hit_group_names[1].empty());
        CHECK(bindings.hit_group_names[2].empty());
        CHECK_EQ(bindings.hit_group_names[3], "__sgl_structural_hit_group_3");
        CHECK(bindings.hit_group_names[4].empty());
        CHECK(bindings.hit_group_names[5].empty());

        REQUIRE_EQ(bindings.miss_entry_points.size(), 5);
        CHECK_EQ(bindings.miss_entry_points[0], exported_names.at("Miss0"));
        CHECK(bindings.miss_entry_points[1].empty());
        CHECK_EQ(bindings.miss_entry_points[2], exported_names.at("Miss2"));
        CHECK(bindings.miss_entry_points[3].empty());
        CHECK(bindings.miss_entry_points[4].empty());

        REQUIRE_EQ(bindings.callable_entry_points.size(), 7);
        CHECK(bindings.callable_entry_points[0].empty());
        CHECK_EQ(bindings.callable_entry_points[4], exported_names.at("Callable4"));
        CHECK(bindings.callable_entry_points[6].empty());

        StructuralRayTracingBindings empty = create_structural_ray_tracing_bindings(
            module.get(),
            "EmptyLayout",
            {
                .min_hit_group_count = 2,
                .min_miss_count = 3,
                .min_callable_count = 4,
            }
        );
        CHECK(empty.entry_points.empty());
        CHECK(empty.hit_groups.empty());
        CHECK_EQ(empty.hit_group_names.size(), 2);
        CHECK_EQ(empty.miss_entry_points.size(), 3);
        CHECK_EQ(empty.callable_entry_points.size(), 4);

        StructuralRayTracingBindings namespaced
            = create_structural_ray_tracing_bindings(module.get(), "NamespacedLayout");
        REQUIRE_EQ(namespaced.entry_points.size(), 1);
        ref<const TraceProgramLayoutInfo> namespaced_layout
            = module->layout()->find_trace_program_layout("NamespacedLayout");
        REQUIRE(namespaced_layout);
        REQUIRE_EQ(namespaced_layout->miss_groups.size(), 1);
        REQUIRE(namespaced_layout->miss_groups[0].miss);
        CHECK_EQ(namespaced.entry_points[0]->desc().name, "NamespacedStages.Miss");
        CHECK_EQ(namespaced.entry_points[0]->name(), namespaced_layout->miss_groups[0].miss->entry_point_name);
        REQUIRE_EQ(namespaced.miss_entry_points.size(), 1);
        CHECK_EQ(namespaced.miss_entry_points[0], namespaced.entry_points[0]->name());
    }

    SUBCASE("adapter validation")
    {
        ref<SlangModule> module
            = session->load_module_from_source("structural_adapter_validation_test", k_structural_source);
        ref<const TraceProgramLayoutInfo> layout = module->layout()->find_trace_program_layout("SparseLayout");
        REQUIRE(layout);
        REQUIRE_EQ(layout->hit_groups.size(), 2);

        ref<TraceProgramLayoutInfo> negative_slot = clone_layout_info(layout.get());
        negative_slot->hit_groups[0].slot = -1;
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(module.get(), negative_slot.get()),
            doctest::Contains("negative slot -1"),
            std::runtime_error
        );

        ref<TraceProgramLayoutInfo> duplicate_slot = clone_layout_info(layout.get());
        duplicate_slot->hit_groups[1].slot = duplicate_slot->hit_groups[0].slot;
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(module.get(), duplicate_slot.get()),
            doctest::Contains("slot 3 is declared more than once"),
            std::runtime_error
        );

        ref<TraceProgramLayoutInfo> non_empty_record = clone_layout_info(layout.get());
        non_empty_record->hit_groups[0].record_type = module->layout()->find_type_by_name("NonEmptyRecord");
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(module.get(), non_empty_record.get()),
            doctest::Contains("uses non-empty shader record type"),
            std::runtime_error
        );

        ref<TraceProgramLayoutInfo> colliding_stage_name = clone_layout_info(layout.get());
        REQUIRE(colliding_stage_name->miss_groups[1].miss);
        colliding_stage_name->miss_groups[1].miss->entry_point_name = "__sgl_structural_hit_group_0";
        StructuralRayTracingBindings collision_safe_bindings
            = create_structural_ray_tracing_bindings(module.get(), colliding_stage_name.get());
        REQUIRE_EQ(collision_safe_bindings.hit_groups.size(), 2);
        CHECK_EQ(collision_safe_bindings.hit_groups[0].hit_group_name, "__sgl_structural_hit_group_0_1");
        REQUIRE_EQ(collision_safe_bindings.miss_entry_points.size(), 3);
        CHECK_EQ(collision_safe_bindings.miss_entry_points[0], "__sgl_structural_hit_group_0");

        ref<SlangModule> other_module
            = session->load_module_from_source("structural_other_module_test", k_structural_source);
        ref<const TraceProgramLayoutInfo> foreign_layout
            = other_module->layout()->find_trace_program_layout("SparseLayout");
        REQUIRE(foreign_layout);
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(module.get(), foreign_layout.get()),
            doctest::Contains("does not belong to module"),
            std::runtime_error
        );
    }
}

TEST_SUITE_END();
