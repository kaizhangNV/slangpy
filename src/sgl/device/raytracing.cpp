// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "raytracing.h"

#include "sgl/device/device.h"
#include "sgl/device/helpers.h"
#include "sgl/device/reflection.h"
#include "sgl/device/shader.h"
#include "sgl/device/shader_cursor.h"

#include "sgl/core/error.h"
#include "sgl/core/type_utils.h"
#include "sgl/core/short_vector.h"

#include <slang-rhi/acceleration-structure-utils.h>

#include <algorithm>
#include <map>
#include <set>

namespace sgl {

AccelerationStructureBuildDescConverter::AccelerationStructureBuildDescConverter(
    const AccelerationStructureBuildDesc& desc
)
{
    for (const auto& input : desc.inputs) {
        if (auto* instances = std::get_if<AccelerationStructureBuildInputInstances>(&input)) {
            rhi::AccelerationStructureBuildInput rhi_build_input{
                .type = rhi::AccelerationStructureBuildInputType::Instances,
                .instances{
                    .instanceBuffer = detail::to_rhi(instances->instance_buffer),
                    .instanceStride = instances->instance_stride,
                    .instanceCount = instances->instance_count,
                },
            };
            rhi_build_inputs.push_back(rhi_build_input);
        } else if (auto* triangles = std::get_if<AccelerationStructureBuildInputTriangles>(&input)) {
            rhi::AccelerationStructureBuildInput rhi_build_input{
                .type = rhi::AccelerationStructureBuildInputType::Triangles,
                .triangles{
                    .vertexBufferCount = narrow_cast<uint32_t>(triangles->vertex_buffers.size()),
                    .vertexFormat = static_cast<rhi::Format>(triangles->vertex_format),
                    .vertexCount = triangles->vertex_count,
                    .vertexStride = triangles->vertex_stride,
                    .indexBuffer = detail::to_rhi(triangles->index_buffer),
                    .indexFormat = static_cast<rhi::IndexFormat>(triangles->index_format),
                    .indexCount = triangles->index_count,
                    .preTransformBuffer = detail::to_rhi(triangles->pre_transform_buffer),
                    .flags = static_cast<rhi::AccelerationStructureGeometryFlags>(triangles->flags),
                },
            };
            for (size_t i = 0; i < triangles->vertex_buffers.size(); ++i)
                rhi_build_input.triangles.vertexBuffers[i] = detail::to_rhi(triangles->vertex_buffers[i]);
            rhi_build_inputs.push_back(rhi_build_input);
        } else if (auto* procedural_primitives
                   = std::get_if<AccelerationStructureBuildInputProceduralPrimitives>(&input)) {
            rhi::AccelerationStructureBuildInput rhi_build_input{
                .type = rhi::AccelerationStructureBuildInputType::ProceduralPrimitives,
                .proceduralPrimitives{
                    .aabbBufferCount = narrow_cast<uint32_t>(procedural_primitives->aabb_buffers.size()),
                    .aabbStride = procedural_primitives->aabb_stride,
                    .primitiveCount = procedural_primitives->primitive_count,
                    .flags = static_cast<rhi::AccelerationStructureGeometryFlags>(procedural_primitives->flags),
                },
            };
            for (size_t i = 0; i < procedural_primitives->aabb_buffers.size(); ++i)
                rhi_build_input.proceduralPrimitives.aabbBuffers[i]
                    = detail::to_rhi(procedural_primitives->aabb_buffers[i]);
            rhi_build_inputs.push_back(rhi_build_input);
        } else if (auto* spheres = std::get_if<AccelerationStructureBuildInputSpheres>(&input)) {
            rhi::AccelerationStructureBuildInput rhi_build_input{
                .type = rhi::AccelerationStructureBuildInputType::Spheres,
                .spheres{
                    .vertexBufferCount = narrow_cast<uint32_t>(spheres->vertex_position_buffers.size()),
                    .vertexCount = spheres->vertex_count,
                    .vertexPositionFormat = static_cast<rhi::Format>(spheres->vertex_position_format),
                    .vertexPositionStride = spheres->vertex_position_stride,
                    .vertexRadiusFormat = static_cast<rhi::Format>(spheres->vertex_radius_format),
                    .vertexRadiusStride = spheres->vertex_radius_stride,
                    .indexBuffer = detail::to_rhi(spheres->index_buffer),
                    .indexFormat = static_cast<rhi::IndexFormat>(spheres->index_format),
                    .indexCount = spheres->index_count,
                    .flags = static_cast<rhi::AccelerationStructureGeometryFlags>(spheres->flags),
                },
            };
            for (size_t i = 0; i < spheres->vertex_position_buffers.size(); ++i)
                rhi_build_input.spheres.vertexPositionBuffers[i] = detail::to_rhi(spheres->vertex_position_buffers[i]);
            for (size_t i = 0; i < spheres->vertex_radius_buffers.size(); ++i)
                rhi_build_input.spheres.vertexRadiusBuffers[i] = detail::to_rhi(spheres->vertex_radius_buffers[i]);
            rhi_build_inputs.push_back(rhi_build_input);
        } else if (auto* linear_swept_spheres
                   = std::get_if<AccelerationStructureBuildInputLinearSweptSpheres>(&input)) {
            rhi::AccelerationStructureBuildInput rhi_build_input{
                .type = rhi::AccelerationStructureBuildInputType::LinearSweptSpheres,
                .linearSweptSpheres{
                    .vertexBufferCount = narrow_cast<uint32_t>(linear_swept_spheres->vertex_position_buffers.size()),
                    .vertexCount = linear_swept_spheres->vertex_count,
                    .primitiveCount = linear_swept_spheres->primitive_count,
                    .vertexPositionFormat = static_cast<rhi::Format>(linear_swept_spheres->vertex_position_format),
                    .vertexPositionStride = linear_swept_spheres->vertex_position_stride,
                    .vertexRadiusFormat = static_cast<rhi::Format>(linear_swept_spheres->vertex_radius_format),
                    .vertexRadiusStride = linear_swept_spheres->vertex_radius_stride,
                    .indexBuffer = detail::to_rhi(linear_swept_spheres->index_buffer),
                    .indexFormat = static_cast<rhi::IndexFormat>(linear_swept_spheres->index_format),
                    .indexCount = linear_swept_spheres->index_count,
                    .indexingMode
                    = static_cast<rhi::LinearSweptSpheresIndexingMode>(linear_swept_spheres->indexing_mode),
                    .endCapsMode = static_cast<rhi::LinearSweptSpheresEndCapsMode>(linear_swept_spheres->end_caps_mode),
                    .flags = static_cast<rhi::AccelerationStructureGeometryFlags>(linear_swept_spheres->flags),
                },
            };
            for (size_t i = 0; i < linear_swept_spheres->vertex_position_buffers.size(); ++i)
                rhi_build_input.linearSweptSpheres.vertexPositionBuffers[i]
                    = detail::to_rhi(linear_swept_spheres->vertex_position_buffers[i]);
            for (size_t i = 0; i < linear_swept_spheres->vertex_radius_buffers.size(); ++i)
                rhi_build_input.linearSweptSpheres.vertexRadiusBuffers[i]
                    = detail::to_rhi(linear_swept_spheres->vertex_radius_buffers[i]);
            rhi_build_inputs.push_back(rhi_build_input);
        }
    }

    rhi_desc.inputs = rhi_build_inputs.data();
    rhi_desc.inputCount = narrow_cast<uint32_t>(rhi_build_inputs.size());

    rhi_desc.motionOptions.keyCount = desc.motion_options.key_count;
    rhi_desc.motionOptions.timeStart = desc.motion_options.time_start;
    rhi_desc.motionOptions.timeEnd = desc.motion_options.time_end;

    rhi_desc.mode = static_cast<rhi::AccelerationStructureBuildMode>(desc.mode);
    rhi_desc.flags = static_cast<rhi::AccelerationStructureBuildFlags>(desc.flags);
}

AccelerationStructure::AccelerationStructure(ref<Device> device, AccelerationStructureDesc desc)
    : DeviceChild(std::move(device))
    , m_desc(std::move(desc))
{
    rhi::AccelerationStructureDesc rhi_desc{
        .kind = static_cast<rhi::AccelerationStructureKind>(desc.kind),
        .size = m_desc.size,
        .label = m_desc.label.c_str(),
    };
    SLANG_RHI_CALL(
        m_device->rhi_device()->createAccelerationStructure(rhi_desc, m_rhi_acceleration_structure.writeRef()),
        m_device
    );
}

AccelerationStructure::~AccelerationStructure() { }

AccelerationStructureHandle AccelerationStructure::handle() const
{
    return m_rhi_acceleration_structure->getHandle();
}

void AccelerationStructure::write_to_cursor(const ShaderCursor& cursor, const AccelerationStructure* value)
{
    cursor.set_acceleration_structure(ref<const AccelerationStructure>(value));
}

std::string AccelerationStructure::to_string() const
{
    return fmt::format(
        "AccelerationStructure(\n"
        "  device = {},\n"
        "  size = {},\n"
        "  label = {}\n",
        ")",
        m_device,
        m_desc.size,
        m_desc.label
    );
}

AccelerationStructureInstanceList::AccelerationStructureInstanceList(ref<Device> device, size_t size)
    : DeviceChild(std::move(device))
{
    m_instance_type = rhi::getAccelerationStructureInstanceDescType(static_cast<rhi::DeviceType>(m_device->type()));
    m_instance_stride = rhi::getAccelerationStructureInstanceDescSize(m_instance_type);
    resize(size);
}

AccelerationStructureInstanceList::~AccelerationStructureInstanceList() { }

void AccelerationStructureInstanceList::resize(size_t size)
{
    m_instances.resize(size);
    m_dirty = true;
}

void AccelerationStructureInstanceList::write(size_t index, const AccelerationStructureInstanceDesc& instance)
{
    m_instances[index] = instance;
    m_dirty = true;
}

void AccelerationStructureInstanceList::write(size_t index, std::span<AccelerationStructureInstanceDesc> instances)
{
    std::copy(instances.begin(), instances.end(), m_instances.begin() + index);
    m_dirty = true;
}

ref<Buffer> AccelerationStructureInstanceList::buffer() const
{
    if (m_dirty) {
        size_t native_size = m_instances.size() * m_instance_stride;

        std::unique_ptr<uint8_t[]> native_descs(new uint8_t[native_size]);

        rhi::convertAccelerationStructureInstanceDescs(
            m_instances.size(),
            m_instance_type,
            native_descs.get(),
            m_instance_stride,
            reinterpret_cast<const rhi::AccelerationStructureInstanceDescGeneric*>(m_instances.data()),
            sizeof(rhi::AccelerationStructureInstanceDescGeneric)
        );

        m_buffer = m_device->create_buffer({
            .usage = BufferUsage::acceleration_structure_build_input,
            .data = native_descs.get(),
            .data_size = native_size,
        });

        m_dirty = false;
    }

    return m_buffer;
}

AccelerationStructureBuildInputInstances AccelerationStructureInstanceList::build_input_instances() const
{
    return AccelerationStructureBuildInputInstances{
        .instance_buffer = BufferOffsetPair{buffer(), 0},
        .instance_stride = narrow_cast<uint32_t>(m_instance_stride),
        .instance_count = narrow_cast<uint32_t>(m_instances.size()),
    };
}

std::string AccelerationStructureInstanceList::to_string() const
{
    return fmt::format(
        "AccelerationStructureInstanceList(\n"
        "  device = {}\n"
        "  size = {}\n"
        ")",
        m_device,
        m_instances.size()
    );
}

namespace {

    constexpr uint32_t k_max_dense_shader_table_entries = 1u << 20;

    template<typename GroupInfo>
    uint32_t
    validate_group_slots(const std::vector<GroupInfo>& groups, uint32_t minimum_count, std::string_view group_kind)
    {
        SGL_CHECK(
            minimum_count <= k_max_dense_shader_table_entries,
            "Requested minimum {} shader-table count {} exceeds the safety limit {}",
            group_kind,
            minimum_count,
            k_max_dense_shader_table_entries
        );

        uint64_t dense_count = minimum_count;
        std::set<uint32_t> slots;
        for (const auto& group : groups) {
            SGL_CHECK(
                group.slot >= 0,
                "Structural ray-tracing {} group \"{}\" has negative slot {}",
                group_kind,
                group.type_name,
                group.slot
            );

            uint64_t slot = static_cast<uint64_t>(group.slot);
            SGL_CHECK(
                slot < k_max_dense_shader_table_entries,
                "Structural ray-tracing {} group \"{}\" slot {} exceeds the safety limit {}",
                group_kind,
                group.type_name,
                slot,
                k_max_dense_shader_table_entries - 1
            );
            SGL_CHECK(
                slots.insert(static_cast<uint32_t>(slot)).second,
                "Structural ray-tracing {} slot {} is declared more than once",
                group_kind,
                slot
            );
            dense_count = std::max(dense_count, slot + 1);
        }
        return narrow_cast<uint32_t>(dense_count);
    }

    void validate_empty_record(
        const ref<const TypeReflection>& record_type,
        std::string_view group_kind,
        std::string_view group_name,
        int64_t slot
    )
    {
        SGL_CHECK(
            record_type,
            "Structural ray-tracing {} group \"{}\" at slot {} has no reflected record type",
            group_kind,
            group_name,
            slot
        );

        bool is_void = record_type->kind() == TypeReflection::Kind::scalar
            && record_type->scalar_type() == TypeReflection::ScalarType::void_;
        bool is_empty_struct = record_type->kind() == TypeReflection::Kind::struct_ && record_type->field_count() == 0;
        SGL_CHECK(
            is_void || is_empty_struct,
            "Structural ray-tracing {} group \"{}\" at slot {} uses non-empty shader record type \"{}\". "
            "Phase 1 supports only void or empty records because SGL does not yet expose shader-record overwrites.",
            group_kind,
            group_name,
            slot,
            record_type->full_name()
        );
    }

} // namespace

StructuralRayTracingBindings create_structural_ray_tracing_bindings(
    const SlangModule* module,
    const TraceProgramLayoutInfo* layout,
    const StructuralRayTracingBindingOptions& options
)
{
    SGL_CHECK(module, "Structural ray-tracing binding creation requires a Slang module");
    SGL_CHECK(layout, "Structural ray-tracing binding creation requires a reflected trace-program layout");
    SGL_CHECK(
        layout->is_valid(),
        "Structural ray-tracing program layout \"{}\" has been invalidated",
        layout->type_name
    );
    SGL_CHECK(
        layout->source_layout->owner() == module,
        "Structural ray-tracing program layout \"{}\" does not belong to module \"{}\"",
        layout->type_name,
        module->name()
    );

    uint32_t hit_group_count = validate_group_slots(layout->hit_groups, options.min_hit_group_count, "hit");
    uint32_t miss_group_count = validate_group_slots(layout->miss_groups, options.min_miss_count, "miss");
    uint32_t callable_group_count
        = validate_group_slots(layout->callable_groups, options.min_callable_count, "callable");

    for (const auto& group : layout->hit_groups)
        validate_empty_record(group.record_type, "hit", group.type_name, group.slot);
    for (const auto& group : layout->miss_groups)
        validate_empty_record(group.record_type, "miss", group.type_name, group.slot);
    for (const auto& group : layout->callable_groups)
        validate_empty_record(group.record_type, "callable", group.type_name, group.slot);

    StructuralRayTracingBindings result;
    result.hit_group_names.resize(hit_group_count);
    result.miss_entry_points.resize(miss_group_count);
    result.callable_entry_points.resize(callable_group_count);

    // slang-rhi resolves entry points and hit groups through one name map. Reserve every reflected
    // stage export before generating hit-group names so a legal user stage such as
    // `__sgl_structural_hit_group_0` cannot alias the group occupying slot zero. `raygen_main` is
    // also reserved for SlangPy's generated ray-generation entry point.
    std::set<std::string> used_pipeline_names{"raygen_main"};
    auto reserve_stage_name = [&](const std::optional<TraceProgramStageInfo>& stage)
    {
        if (stage && !stage->entry_point_name.empty())
            used_pipeline_names.insert(stage->entry_point_name);
    };
    for (const auto& group : layout->hit_groups) {
        reserve_stage_name(group.closest_hit);
        reserve_stage_name(group.any_hit);
        reserve_stage_name(group.intersection);
    }
    for (const auto& group : layout->miss_groups)
        reserve_stage_name(group.miss);
    for (const auto& group : layout->callable_groups)
        reserve_stage_name(group.callable);

    std::map<std::pair<std::string, ShaderStage>, ref<SlangEntryPoint>> resolved_entry_points;
    auto resolve_stage = [&](const TraceProgramStageInfo& stage, ShaderStage expected_stage) -> std::string
    {
        SGL_CHECK(
            stage.stage == expected_stage,
            "Structural ray-tracing stage type \"{}\" reflects stage {}, but {} was expected",
            stage.type_name,
            stage.stage,
            expected_stage
        );
        SGL_CHECK(
            !stage.type_name.empty(),
            "Structural ray-tracing stage has no reflected type name for checked materialization"
        );
        SGL_CHECK(
            !stage.entry_point_name.empty(),
            "Structural ray-tracing stage type \"{}\" has no reflected native entry-point name",
            stage.type_name
        );

        // Checked materialization needs the fully qualified reflected type identity; the reflected
        // public entry-point name below is the target-facing export identity.
        auto key = std::make_pair(stage.type_name, stage.stage);
        auto iterator = resolved_entry_points.find(key);
        if (iterator == resolved_entry_points.end()) {
            ref<SlangEntryPoint> entry_point = module->checked_entry_point(stage.type_name, stage.stage);
            // Slang reflection supplies the deterministic public name used when the same stage is
            // synthesized from a trace operation. Apply it to the separately materialized stage so
            // per-entry-point target compilation and shader-table lookup use the same identity.
            entry_point = entry_point->with_name(stage.entry_point_name);
            iterator = resolved_entry_points.emplace(std::move(key), entry_point).first;
            result.entry_points.push_back(std::move(entry_point));
        }
        return iterator->second->name();
    };

    std::vector<const TraceProgramHitGroupInfo*> sorted_hit_groups;
    sorted_hit_groups.reserve(layout->hit_groups.size());
    for (const auto& group : layout->hit_groups)
        sorted_hit_groups.push_back(&group);
    std::sort(
        sorted_hit_groups.begin(),
        sorted_hit_groups.end(),
        [](const auto* left, const auto* right)
        {
            return left->slot < right->slot;
        }
    );

    result.hit_groups.reserve(sorted_hit_groups.size());
    for (const TraceProgramHitGroupInfo* group : sorted_hit_groups) {
        std::string hit_group_name_base = fmt::format("__sgl_structural_hit_group_{}", group->slot);
        std::string hit_group_name = hit_group_name_base;
        uint32_t collision_index = 0;
        while (!used_pipeline_names.insert(hit_group_name).second) {
            ++collision_index;
            hit_group_name = fmt::format("{}_{}", hit_group_name_base, collision_index);
        }
        HitGroupDesc desc;
        desc.hit_group_name = hit_group_name;
        if (group->closest_hit)
            desc.closest_hit_entry_point = resolve_stage(*group->closest_hit, ShaderStage::closest_hit);
        if (group->any_hit)
            desc.any_hit_entry_point = resolve_stage(*group->any_hit, ShaderStage::any_hit);
        if (group->intersection)
            desc.intersection_entry_point = resolve_stage(*group->intersection, ShaderStage::intersection);
        result.hit_group_names[narrow_cast<size_t>(group->slot)] = std::move(hit_group_name);
        result.hit_groups.push_back(std::move(desc));
    }

    std::vector<const TraceProgramMissGroupInfo*> sorted_miss_groups;
    sorted_miss_groups.reserve(layout->miss_groups.size());
    for (const auto& group : layout->miss_groups)
        sorted_miss_groups.push_back(&group);
    std::sort(
        sorted_miss_groups.begin(),
        sorted_miss_groups.end(),
        [](const auto* left, const auto* right)
        {
            return left->slot < right->slot;
        }
    );
    for (const TraceProgramMissGroupInfo* group : sorted_miss_groups) {
        SGL_CHECK(
            group->miss.has_value(),
            "Structural ray-tracing miss group \"{}\" at slot {} has no miss stage",
            group->type_name,
            group->slot
        );
        result.miss_entry_points[narrow_cast<size_t>(group->slot)] = resolve_stage(*group->miss, ShaderStage::miss);
    }

    std::vector<const TraceProgramCallableGroupInfo*> sorted_callable_groups;
    sorted_callable_groups.reserve(layout->callable_groups.size());
    for (const auto& group : layout->callable_groups)
        sorted_callable_groups.push_back(&group);
    std::sort(
        sorted_callable_groups.begin(),
        sorted_callable_groups.end(),
        [](const auto* left, const auto* right)
        {
            return left->slot < right->slot;
        }
    );
    for (const TraceProgramCallableGroupInfo* group : sorted_callable_groups) {
        SGL_CHECK(
            group->callable.has_value(),
            "Structural ray-tracing callable group \"{}\" at slot {} has no callable stage",
            group->type_name,
            group->slot
        );
        result.callable_entry_points[narrow_cast<size_t>(group->slot)]
            = resolve_stage(*group->callable, ShaderStage::callable);
    }

    return result;
}

StructuralRayTracingBindings create_structural_ray_tracing_bindings(
    const SlangModule* module,
    std::string_view layout_name,
    const StructuralRayTracingBindingOptions& options
)
{
    SGL_CHECK(module, "Structural ray-tracing binding creation requires a Slang module");
    ref<const TraceProgramLayoutInfo> layout = module->layout()->find_trace_program_layout(layout_name);
    SGL_CHECK(
        layout,
        "Structural ray-tracing program layout \"{}\" was not found in module \"{}\"",
        layout_name,
        module->name()
    );
    return create_structural_ray_tracing_bindings(module, layout.get(), options);
}

ShaderTable::ShaderTable(ref<Device> device, ShaderTableDesc desc)
    : DeviceChild(std::move(device))
{
    short_vector<const char*, 16> rhi_ray_gen_entry_points;
    rhi_ray_gen_entry_points.reserve(desc.ray_gen_entry_points.size());
    for (const auto& name : desc.ray_gen_entry_points)
        rhi_ray_gen_entry_points.push_back(name.c_str());

    short_vector<const char*, 16> rhi_miss_entry_points;
    rhi_miss_entry_points.reserve(desc.miss_entry_points.size());
    for (const auto& name : desc.miss_entry_points)
        rhi_miss_entry_points.push_back(name.c_str());

    short_vector<const char*, 16> rhi_hit_group_names;
    rhi_hit_group_names.reserve(desc.hit_group_names.size());
    for (const auto& name : desc.hit_group_names)
        rhi_hit_group_names.push_back(name.c_str());

    short_vector<const char*, 16> rhi_callable_names;
    rhi_callable_names.reserve(desc.callable_entry_points.size());
    for (const auto& name : desc.callable_entry_points)
        rhi_callable_names.push_back(name.c_str());

    rhi::ShaderTableDesc rhi_desc{
        .rayGenShaderCount = narrow_cast<uint32_t>(rhi_ray_gen_entry_points.size()),
        .rayGenShaderEntryPointNames = rhi_ray_gen_entry_points.data(),
        .rayGenShaderRecordOverwrites = nullptr,
        .missShaderCount = narrow_cast<uint32_t>(rhi_miss_entry_points.size()),
        .missShaderEntryPointNames = rhi_miss_entry_points.data(),
        .missShaderRecordOverwrites = nullptr,
        .hitGroupCount = narrow_cast<uint32_t>(rhi_hit_group_names.size()),
        .hitGroupNames = rhi_hit_group_names.data(),
        .hitGroupRecordOverwrites = nullptr,
        .callableShaderCount = narrow_cast<uint32_t>(rhi_callable_names.size()),
        .callableShaderEntryPointNames = rhi_callable_names.data(),
        .callableShaderRecordOverwrites = nullptr,
        .program = desc.program->rhi_shader_program(),
    };

    SLANG_RHI_CALL(m_device->rhi_device()->createShaderTable(rhi_desc, m_rhi_shader_table.writeRef()), m_device);
}

ShaderTable::~ShaderTable() { }

std::string ShaderTable::to_string() const
{
    return fmt::format(
        "ShaderTable(\n"
        "  device = {}\n"
        ")",
        m_device
    );
}

} // namespace sgl
