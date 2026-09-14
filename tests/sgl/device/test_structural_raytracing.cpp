// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "testing.h"

#include "sgl/device/device.h"
#include "sgl/device/raytracing.h"
#include "sgl/device/reflection.h"
#include "sgl/device/shader.h"

using namespace sgl;

namespace {

constexpr std::string_view k_structural_source = R"SLANG(
import slang.raytracing;

struct RadiancePayload { uint value; }
struct ShadowPayload { uint value; }
struct TestRecord { uint value; }

struct TraceContext : rt::ITraceContext
{
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}

struct RadianceHitContext : rt::IHitContext
{
    typealias TraceContext = ::TraceContext;
    typealias Payload = RadiancePayload;
    typealias Primitive = rt::TrianglePrimitive;
    typealias Record = TestRecord;
}

struct ShadowHitContext : rt::IHitContext
{
    typealias TraceContext = ::TraceContext;
    typealias Payload = ShadowPayload;
    typealias Primitive = rt::TrianglePrimitive;
    typealias Record = TestRecord;
}

struct RadianceClosestHit : rt::IClosestHitShader
{
    typealias Context = RadianceHitContext;
    void invoke(rt::ClosestHitInput<Context> input) { input.payload.value = input.record.value; }
}

struct ShadowClosestHit : rt::IClosestHitShader
{
    typealias Context = ShadowHitContext;
    void invoke(rt::ClosestHitInput<Context> input) { input.payload.value = input.record.value; }
}

struct RadianceHitGroup : rt::IHitGroup
{
    typealias Context = RadianceHitContext;
    typealias ClosestHit = RadianceClosestHit;
    typealias AnyHit = rt::NoAnyHit<Context>;
    typealias Intersection = rt::NoIntersection<Context>;
}

struct ShadowHitGroup : rt::IHitGroup
{
    typealias Context = ShadowHitContext;
    typealias ClosestHit = ShadowClosestHit;
    typealias AnyHit = rt::NoAnyHit<Context>;
    typealias Intersection = rt::NoIntersection<Context>;
}

struct RadianceMissContext : rt::IPayloadContext
{
    typealias TraceContext = ::TraceContext;
    typealias Payload = RadiancePayload;
    typealias Record = TestRecord;
}

struct ShadowMissContext : rt::IPayloadContext
{
    typealias TraceContext = ::TraceContext;
    typealias Payload = ShadowPayload;
    typealias Record = TestRecord;
}

struct RadianceMiss : rt::IMissShader
{
    typealias Context = RadianceMissContext;
    void invoke(rt::MissInput<Context> input) { input.payload.value = input.record.value; }
}

struct ShadowMiss : rt::IMissShader
{
    typealias Context = ShadowMissContext;
    void invoke(rt::MissInput<Context> input) { input.payload.value = input.record.value; }
}

struct CallableContext : rt::ICallableContext
{
    typealias TraceContext = ::TraceContext;
    typealias CallableData = uint;
    typealias Record = TestRecord;
}

struct Callable : rt::ICallableShader
{
    typealias Context = CallableContext;
    void invoke(rt::CallableInput<Context> input) { input.data += input.record.value; }
}

struct TestSchema : rt::ITraceProgramSchema
{
    typealias TraceContext = ::TraceContext;
    typealias HitGroups = rt::HitGroupList<RadianceHitGroup, ShadowHitGroup>;
    typealias MissShaders = rt::MissShaderList<RadianceMiss, ShadowMiss>;
    typealias CallableShaders = rt::CallableShaderList<Callable>;
}

rt::TraceProgramDescriptor<TestSchema> testProgram;

[shader("compute")]
[numthreads(1, 1, 1)]
void explicit_compute() {}

[shader("raygeneration")]
void __sgl_structural_empty_hit_group() {}
)SLANG";

constexpr std::string_view k_ambiguous_stage_source = R"SLANG(
import slang.raytracing;

struct DuplicatePayload { uint value; }

struct DuplicateTraceContext : rt::ITraceContext
{
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}

struct DuplicateMissContext : rt::IPayloadContext
{
    typealias TraceContext = DuplicateTraceContext;
    typealias Payload = DuplicatePayload;
    typealias Record = void;
}

struct DuplicateMiss : rt::IMissShader
{
    typealias Context = DuplicateMissContext;
    void invoke(rt::MissInput<Context> input) {}
}
)SLANG";

constexpr std::string_view k_open_schema_source = R"SLANG(
module structural_open_schema_bridge;
import slang.raytracing;

public struct OpenPayload { public uint value; }
public struct OpenTraceContext : rt::ITraceContext
{
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}
public interface IOpenHitGroup : rt::IHitGroup {}
public struct OpenSchema : rt::ITraceProgramSchema
{
    typealias TraceContext = OpenTraceContext;
    typealias HitGroups = rt::OpenHitGroups<IOpenHitGroup>;
    typealias MissShaders = rt::NoMissShaders;
    typealias CallableShaders = rt::NoCallableShaders;
}
public rt::TraceProgramDescriptor<OpenSchema> open_program;
)SLANG";

constexpr std::string_view k_open_plugin_source = R"SLANG(
module structural_open_schema_plugin;
import slang.raytracing;
import structural_open_schema_bridge;

struct PluginHitContext : rt::IHitContext
{
    typealias TraceContext = OpenTraceContext;
    typealias Payload = OpenPayload;
    typealias Primitive = rt::TrianglePrimitive;
    typealias Record = void;
}
struct PluginClosestHit : rt::IClosestHitShader
{
    typealias Context = PluginHitContext;
    void invoke(rt::ClosestHitInput<Context> input) { input.payload.value = 7; }
}
public struct PluginHitGroup : IOpenHitGroup
{
    typealias Context = PluginHitContext;
    typealias ClosestHit = PluginClosestHit;
    typealias AnyHit = rt::NoAnyHit<Context>;
    typealias Intersection = rt::NoIntersection<Context>;
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

const TraceProgramPayloadInfo* find_payload(const TraceProgramSchemaInfo* schema, std::string_view type_name)
{
    for (const auto& payload : schema->payloads) {
        if (payload.type_name == type_name)
            return &payload;
    }
    return nullptr;
}

ref<TraceProgramSchemaInfo> clone_schema_info(const TraceProgramSchemaInfo* source)
{
    auto result = make_ref<TraceProgramSchemaInfo>();
    result->source_layout = source->source_layout;
    result->name = source->name;
    result->type = source->type;
    result->type_name = source->type_name;
    result->trace_context_type = source->trace_context_type;
    result->is_hit_group_section_open = source->is_hit_group_section_open;
    result->is_miss_shader_section_open = source->is_miss_shader_section_open;
    result->is_callable_shader_section_open = source->is_callable_shader_section_open;
    result->hit_record_stride = source->hit_record_stride;
    result->miss_record_stride = source->miss_record_stride;
    result->callable_record_stride = source->callable_record_stride;
    result->max_native_hit_attribute_size = source->max_native_hit_attribute_size;
    result->metal_record_header_size = source->metal_record_header_size;
    result->payloads = source->payloads;
    result->callable_shaders = source->callable_shaders;
    return result;
}

} // namespace

TEST_SUITE_BEGIN("device");

TEST_CASE_GPU("structural ray tracing native bridge")
{
    ref<sgl::SlangSession> session = create_structural_session(ctx.device);

    SUBCASE("schema reflection snapshots payload partitions")
    {
        ref<SlangModule> module = session->load_module_from_source("structural_schema_reflection", k_structural_source);
        ref<const ProgramLayout> program_layout = module->layout();
        ref<const TraceProgramSchemaInfo> schema = program_layout->find_trace_program_schema("TestSchema");

        REQUIRE(schema);
        CHECK(schema->is_valid());
        CHECK_EQ(schema->name, "TestSchema");
        CHECK_EQ(schema->type_name, "TestSchema");
        REQUIRE(schema->trace_context_type);
        CHECK_EQ(schema->trace_context_type->full_name(), "TraceContext");
        REQUIRE_EQ(schema->payloads.size(), 2);

        const auto* radiance = find_payload(schema.get(), "RadiancePayload");
        const auto* shadow = find_payload(schema.get(), "ShadowPayload");
        REQUIRE(radiance);
        REQUIRE(shadow);
        REQUIRE_EQ(radiance->hit_groups.size(), 1);
        REQUIRE_EQ(shadow->hit_groups.size(), 1);
        REQUIRE_EQ(radiance->miss_shaders.size(), 1);
        REQUIRE_EQ(shadow->miss_shaders.size(), 1);
        CHECK_FALSE(radiance->hit_groups[0].is_linked);
        CHECK_FALSE(radiance->miss_shaders[0].is_linked);

        // Function indices are local to a payload partition. Both zero values are valid and must
        // never be interpreted as one schema-wide physical record index.
        CHECK_EQ(radiance->hit_groups[0].function_index, 0);
        CHECK_EQ(shadow->hit_groups[0].function_index, 0);
        CHECK_EQ(radiance->miss_shaders[0].function_index, 0);
        CHECK_EQ(shadow->miss_shaders[0].function_index, 0);
        REQUIRE(radiance->hit_groups[0].record_type_layout);
        CHECK_EQ(radiance->hit_groups[0].record_type_layout->size(), 4);
        REQUIRE_EQ(schema->callable_shaders.size(), 1);
        CHECK_EQ(schema->callable_shaders[0].function_index, 0);
        CHECK_FALSE(program_layout->find_trace_program_schema("TraceContext"));

        program_layout.reset();
        module.reset();
        CHECK(schema->is_valid());
        CHECK_EQ(radiance->hit_groups[0].type_name, "RadianceHitGroup");
    }

    SUBCASE("linked open-schema entries are selectable")
    {
        ref<SlangModule> schema_module
            = session->load_module_from_source("structural_open_schema_bridge", k_open_schema_source);
        ref<SlangModule> plugin_module
            = session->load_module_from_source("structural_open_schema_plugin", k_open_plugin_source);
        ref<SlangModule> composed
            = session->compose_modules("structural_open_schema_composed", {schema_module, plugin_module});
        ref<const TraceProgramSchemaInfo> schema = composed->layout()->find_trace_program_schema("OpenSchema");
        REQUIRE(schema);
        CHECK(schema->is_hit_group_section_open);
        REQUIRE_EQ(schema->payloads.size(), 1);
        REQUIRE_EQ(schema->payloads[0].hit_groups.size(), 1);
        const auto& plugin_group = schema->payloads[0].hit_groups[0];
        CHECK(plugin_group.is_linked);
        CHECK(plugin_group.type_name.find("PluginHitGroup") != std::string::npos);

        StructuralRayTracingBindings bindings = create_structural_ray_tracing_bindings(
            composed.get(),
            schema.get(),
            {.hit_group_types = {plugin_group.type_name}}
        );
        REQUIRE_EQ(bindings.hit_group_names.size(), 1);
        CHECK_FALSE(bindings.hit_group_names[0].empty());
        REQUIRE_EQ(bindings.entry_points.size(), 1);
        CHECK_EQ(bindings.entry_points[0]->stage(), ShaderStage::closest_hit);
    }

    SUBCASE("checked synthesized entry point lookup")
    {
        ref<SlangModule> module
            = session->load_module_from_source("structural_schema_entry_point", k_structural_source);
        ref<sgl::SlangEntryPoint> closest_hit
            = module->checked_entry_point("RadianceClosestHit", ShaderStage::closest_hit);
        REQUIRE(closest_hit);
        CHECK_EQ(closest_hit->stage(), ShaderStage::closest_hit);
        CHECK_EQ(closest_hit->name(), "RadianceClosestHit");

        ref<sgl::SlangEntryPoint> renamed_closest_hit = closest_hit->with_name("RenamedRadianceClosestHit");
        REQUIRE(renamed_closest_hit);
        CHECK_EQ(renamed_closest_hit->name(), "RenamedRadianceClosestHit");
        CHECK_EQ(renamed_closest_hit->stage(), ShaderStage::closest_hit);
        CHECK_EQ(renamed_closest_hit->desc().name, "RadianceClosestHit");
        REQUIRE(renamed_closest_hit->desc().requested_stage);
        CHECK_EQ(*renamed_closest_hit->desc().requested_stage, ShaderStage::closest_hit);

        ref<sgl::SlangEntryPoint> renamed_again = renamed_closest_hit->with_name("RenamedAgainRadianceClosestHit");
        REQUIRE(renamed_again);
        CHECK_EQ(renamed_again->name(), "RenamedAgainRadianceClosestHit");
        CHECK_EQ(renamed_again->desc().name, "RadianceClosestHit");

        ref<sgl::SlangEntryPoint> specialized = renamed_closest_hit->specialize(std::span<const SpecializationArg>{});
        REQUIRE(specialized);
        CHECK_EQ(specialized->name(), "RenamedRadianceClosestHit");
        CHECK_EQ(specialized->stage(), ShaderStage::closest_hit);

        CHECK_THROWS_WITH_AS(
            module->checked_entry_point("RadianceClosestHit", ShaderStage::miss),
            doctest::Contains("stage mismatch"),
            std::runtime_error
        );

        ref<sgl::SlangEntryPoint> legacy = module->entry_point("explicit_compute");
        REQUIRE(legacy);
        CHECK_EQ(legacy->stage(), ShaderStage::compute);

        ref<sgl::SlangEntryPoint> renamed_legacy = legacy->with_name("renamed_explicit_compute");
        REQUIRE(renamed_legacy);
        CHECK_EQ(renamed_legacy->name(), "renamed_explicit_compute");
        CHECK_EQ(renamed_legacy->desc().name, "explicit_compute");
    }

    SUBCASE("composed module resolution and hot reload retention")
    {
        ref<SlangModule> module
            = session->load_module_from_source("structural_schema_composed_stage", k_structural_source);
        ref<SlangModule> support = session->load_module_from_source(
            "structural_schema_composed_support",
            R"SLANG(
interface IStructuralSupport { uint value(); }
struct StructuralSupport : IStructuralSupport { uint value() { return 1; } }
export uint structural_support_value() { return 1; }
)SLANG"
        );
        ref<SlangModule> composed = session->compose_modules("structural_schema_composed", {support, module});
        ref<SlangModule> nested = session->compose_modules("structural_schema_nested_composed", {support, composed});

        ref<sgl::SlangEntryPoint> entry_point = nested->checked_entry_point("RadianceMiss", ShaderStage::miss);
        REQUIRE(entry_point);
        CHECK_EQ(entry_point->module(), module.get());
        CHECK_EQ(entry_point->stage(), ShaderStage::miss);

        const TypeConformance support_conformance{"IStructuralSupport", "StructuralSupport", 7};
        const std::span<const TypeConformance> support_conformances(&support_conformance, 1);
        ref<sgl::SlangEntryPoint> conformance_entry_point
            = composed->checked_entry_point("RadianceMiss", ShaderStage::miss, support_conformances);
        REQUIRE(conformance_entry_point);
        CHECK_EQ(conformance_entry_point->module(), module.get());
        CHECK_EQ(conformance_entry_point->stage(), ShaderStage::miss);

        ref<sgl::SlangEntryPoint> legacy_conformance_entry_point
            = composed->entry_point("explicit_compute", support_conformances);
        REQUIRE(legacy_conformance_entry_point);
        CHECK_EQ(legacy_conformance_entry_point->module(), module.get());
        CHECK_EQ(legacy_conformance_entry_point->stage(), ShaderStage::compute);

        ref<SlangModule> structural_program
            = session->compose_modules("structural_schema_composed_conformance", {composed}, support_conformances);
        StructuralRayTracingBindings bindings = create_structural_ray_tracing_bindings(
            structural_program.get(),
            "TestSchema",
            {
                .hit_group_types = {"RadianceHitGroup", "ShadowHitGroup"},
                .miss_shader_types = {"RadianceMiss", "ShadowMiss"},
                .callable_shader_types = {"Callable"},
            }
        );
        REQUIRE_EQ(bindings.entry_points.size(), 5);

        std::vector<std::pair<std::string, ShaderStage>> expected_entry_points;
        for (const auto& structural_entry_point : bindings.entry_points)
            expected_entry_points.emplace_back(structural_entry_point->name(), structural_entry_point->stage());
        ref<ShaderProgram> retained_program = session->link_program({structural_program}, bindings.entry_points);
        REQUIRE(retained_program);

        session->recreate_session();

        REQUIRE_EQ(bindings.entry_points.size(), expected_entry_points.size());
        for (size_t index = 0; index < expected_entry_points.size(); ++index) {
            CHECK_EQ(bindings.entry_points[index]->name(), expected_entry_points[index].first);
            CHECK_EQ(bindings.entry_points[index]->stage(), expected_entry_points[index].second);
        }
        REQUIRE(retained_program->layout());

        ref<SlangModule> duplicate_a
            = session->load_module_from_source("structural_schema_duplicate_a", k_ambiguous_stage_source);
        ref<SlangModule> duplicate_b
            = session->load_module_from_source("structural_schema_duplicate_b", k_ambiguous_stage_source);
        ref<SlangModule> ambiguous
            = session->compose_modules("structural_schema_ambiguous", {duplicate_a, duplicate_b});
        CHECK_THROWS_WITH_AS(
            ambiguous->checked_entry_point("DuplicateMiss", ShaderStage::miss),
            doctest::Contains("ambiguous across composed source modules"),
            std::runtime_error
        );
    }

    SUBCASE("host physical records preserve order duplicates and bytes")
    {
        ref<SlangModule> module = session->load_module_from_source("structural_schema_adapter", k_structural_source);
        StructuralRayTracingBindings bindings = create_structural_ray_tracing_bindings(
            module.get(),
            "TestSchema",
            {
                .hit_group_types = {"ShadowHitGroup", "RadianceHitGroup", "RadianceHitGroup", ""},
                .miss_shader_types = {"ShadowMiss", "RadianceMiss"},
                .callable_shader_types = {"Callable", "Callable"},
                .hit_group_record_data = {{1, 0, 0, 0}, {2, 0, 0, 0}, {3, 0, 0, 0}, {}},
                .miss_shader_record_data = {{4, 0, 0, 0}, {5, 0, 0, 0}},
                .callable_shader_record_data = {{6, 0, 0, 0}, {7, 0, 0, 0}},
            }
        );

        REQUIRE_EQ(bindings.entry_points.size(), 5);
        REQUIRE_EQ(bindings.hit_groups.size(), 3); // two schema functions plus one empty-record group
        REQUIRE_EQ(bindings.hit_group_names.size(), 4);
        CHECK_NE(bindings.hit_group_names[0], bindings.hit_group_names[1]);
        CHECK_EQ(bindings.hit_group_names[1], bindings.hit_group_names[2]);
        CHECK_FALSE(bindings.hit_group_names[3].empty());
        CHECK_EQ(bindings.hit_group_names[3], "__sgl_structural_empty_hit_group_1");
        CHECK_EQ(bindings.hit_group_record_data[0], std::vector<uint8_t>({1, 0, 0, 0}));
        CHECK_EQ(bindings.hit_group_record_data[2], std::vector<uint8_t>({3, 0, 0, 0}));
        CHECK(bindings.hit_group_record_data[3].empty());

        REQUIRE_EQ(bindings.miss_entry_points.size(), 2);
        CHECK_NE(bindings.miss_entry_points[0], bindings.miss_entry_points[1]);
        REQUIRE_EQ(bindings.callable_entry_points.size(), 2);
        CHECK_EQ(bindings.callable_entry_points[0], bindings.callable_entry_points[1]);
        CHECK_EQ(bindings.callable_shader_record_data[1], std::vector<uint8_t>({7, 0, 0, 0}));
        CHECK_EQ(bindings.max_ray_payload_size, 4);
        CHECK_EQ(bindings.max_attribute_size, 8);

        StructuralRayTracingBindings zero_initialized = create_structural_ray_tracing_bindings(
            module.get(),
            "TestSchema",
            {
                .hit_group_types = {"RadianceHitGroup"},
                .miss_shader_types = {"RadianceMiss"},
            }
        );
        CHECK_EQ(zero_initialized.hit_group_record_data[0], std::vector<uint8_t>(4, 0));
        CHECK_EQ(zero_initialized.miss_shader_record_data[0], std::vector<uint8_t>(4, 0));
    }

    SUBCASE("adapter rejects invalid host records")
    {
        ref<SlangModule> module = session->load_module_from_source("structural_schema_validation", k_structural_source);

        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(module.get(), "TestSchema", {.hit_group_types = {"NotInSchema"}}),
            doctest::Contains("is not a member of schema"),
            std::runtime_error
        );
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(
                module.get(),
                "TestSchema",
                {
                    .hit_group_types = {"RadianceHitGroup"},
                    .hit_group_record_data = {{1, 2}},
                }
            ),
            doctest::Contains("requires 4"),
            std::runtime_error
        );
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(
                module.get(),
                "TestSchema",
                {
                    .hit_group_types = {"RadianceHitGroup", "ShadowHitGroup"},
                    .hit_group_record_data = {{1, 0, 0, 0}},
                }
            ),
            doctest::Contains("record-data count 1"),
            std::runtime_error
        );
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(
                module.get(),
                "TestSchema",
                {
                    .hit_group_types = {""},
                    .hit_group_record_data = {{1}},
                }
            ),
            doctest::Contains("cannot carry application data"),
            std::runtime_error
        );

        ref<SlangModule> other_module
            = session->load_module_from_source("structural_schema_foreign", k_structural_source);
        ref<const TraceProgramSchemaInfo> foreign_schema
            = other_module->layout()->find_trace_program_schema("TestSchema");
        REQUIRE(foreign_schema);
        CHECK_THROWS_WITH_AS(
            create_structural_ray_tracing_bindings(module.get(), foreign_schema.get()),
            doctest::Contains("does not belong to module"),
            std::runtime_error
        );

        ref<const TraceProgramSchemaInfo> schema = module->layout()->find_trace_program_schema("TestSchema");
        REQUIRE(schema);
        ref<TraceProgramSchemaInfo> colliding_stage_name = clone_schema_info(schema.get());
        bool renamed = false;
        for (auto& payload : colliding_stage_name->payloads) {
            for (auto& shader : payload.miss_shaders) {
                if (shader.type_name == "RadianceMiss") {
                    REQUIRE(shader.miss);
                    shader.miss->entry_point_name = "__sgl_structural_hit_group_p0_f0";
                    renamed = true;
                }
            }
        }
        REQUIRE(renamed);
        StructuralRayTracingBindings collision_safe = create_structural_ray_tracing_bindings(
            module.get(),
            colliding_stage_name.get(),
            {
                .hit_group_types = {"RadianceHitGroup"},
                .miss_shader_types = {"RadianceMiss"},
            }
        );
        REQUIRE_EQ(collision_safe.hit_groups.size(), 2);
        CHECK_EQ(collision_safe.hit_groups[0].hit_group_name, "__sgl_structural_hit_group_p0_f0_1");
        REQUIRE_EQ(collision_safe.miss_entry_points.size(), 1);
        CHECK_EQ(collision_safe.miss_entry_points[0], "__sgl_structural_hit_group_p0_f0");

        ref<TraceProgramSchemaInfo> metal_noop_closest_hit = clone_schema_info(schema.get());
        auto* shadow
            = const_cast<TraceProgramPayloadInfo*>(find_payload(metal_noop_closest_hit.get(), "ShadowPayload"));
        REQUIRE(shadow);
        REQUIRE_EQ(shadow->hit_groups.size(), 1);
        shadow->hit_groups[0].closest_hit.reset();
        shadow->hit_groups[0].closest_hit_entry_point_name = "__structural_metal_noop_closest_hit";
        StructuralRayTracingBindings metal_noop = create_structural_ray_tracing_bindings(
            module.get(),
            metal_noop_closest_hit.get(),
            {.hit_group_types = {"ShadowHitGroup"}}
        );
        REQUIRE_EQ(metal_noop.hit_groups.size(), 2);
        CHECK_EQ(metal_noop.hit_groups[1].closest_hit_entry_point, "__structural_metal_noop_closest_hit");

        ref<TraceProgramSchemaInfo> metal_folded_candidates = clone_schema_info(schema.get());
        auto* radiance
            = const_cast<TraceProgramPayloadInfo*>(find_payload(metal_folded_candidates.get(), "RadiancePayload"));
        REQUIRE(radiance);
        REQUIRE_EQ(radiance->hit_groups.size(), 1);
        REQUIRE(radiance->hit_groups[0].closest_hit);
        TraceProgramStageInfo folded_any_hit = *radiance->hit_groups[0].closest_hit;
        folded_any_hit.stage = ShaderStage::any_hit;
        folded_any_hit.type_name = "MetalFoldedAnyHit";
        folded_any_hit.entry_point_name.clear();
        radiance->hit_groups[0].any_hit = folded_any_hit;
        TraceProgramStageInfo folded_intersection = folded_any_hit;
        folded_intersection.stage = ShaderStage::intersection;
        folded_intersection.type_name = "MetalFoldedIntersection";
        radiance->hit_groups[0].intersection = folded_intersection;
        if (ctx.device->info().type == DeviceType::metal) {
            StructuralRayTracingBindings folded_candidates = create_structural_ray_tracing_bindings(
                module.get(),
                metal_folded_candidates.get(),
                {.hit_group_types = {"RadianceHitGroup"}}
            );
            REQUIRE_EQ(folded_candidates.hit_groups.size(), 2);
            CHECK(folded_candidates.hit_groups[0].any_hit_entry_point.empty());
            CHECK(folded_candidates.hit_groups[0].intersection_entry_point.empty());
        } else {
            CHECK_THROWS_WITH_AS(
                create_structural_ray_tracing_bindings(
                    module.get(),
                    metal_folded_candidates.get(),
                    {.hit_group_types = {"RadianceHitGroup"}}
                ),
                doctest::Contains("has no reflected native entry-point name on a non-Metal target"),
                std::runtime_error
            );
        }
    }
}

TEST_SUITE_END();
