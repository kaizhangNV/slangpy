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
#include <span>

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
    constexpr size_t k_max_shader_record_data_size = 1u << 20;

    void validate_physical_records(
        std::span<const std::string> types,
        std::span<const std::vector<uint8_t>> record_data,
        std::string_view kind
    )
    {
        SGL_CHECK(
            types.size() <= k_max_dense_shader_table_entries,
            "Requested {} shader-table count {} exceeds the safety limit {}",
            kind,
            types.size(),
            k_max_dense_shader_table_entries
        );
        SGL_CHECK(
            record_data.empty() || record_data.size() == types.size(),
            "Structural ray-tracing {} record-data count {} must be zero or match the physical record count {}",
            kind,
            record_data.size(),
            types.size()
        );
    }

    size_t reflected_record_size(
        const ref<const TypeReflection>& record_type,
        const ref<const TypeLayoutReflection>& record_type_layout,
        std::string_view kind,
        std::string_view type_name
    )
    {
        SGL_CHECK(record_type, "Structural ray-tracing {} \"{}\" has no reflected record type", kind, type_name);

        const bool is_void = record_type->kind() == TypeReflection::Kind::scalar
            && record_type->scalar_type() == TypeReflection::ScalarType::void_;
        if (is_void)
            return 0;

        SGL_CHECK(
            record_type_layout,
            "Structural ray-tracing {} \"{}\" has no reflected record layout",
            kind,
            type_name
        );
        const size_t size = record_type_layout->size();
        SGL_CHECK(
            size <= k_max_shader_record_data_size,
            "Structural ray-tracing {} \"{}\" record size {} exceeds the safety limit {}",
            kind,
            type_name,
            size,
            k_max_shader_record_data_size
        );
        return size;
    }

} // namespace

StructuralRayTracingBindings create_structural_ray_tracing_bindings(
    const SlangModule* module,
    const TraceProgramSchemaInfo* schema,
    const StructuralRayTracingBindingOptions& options
)
{
    SGL_CHECK(module, "Structural ray-tracing binding creation requires a Slang module");
    SGL_CHECK(schema, "Structural ray-tracing binding creation requires a reflected trace-program schema");
    SGL_CHECK(schema->is_valid(), "Structural ray-tracing program schema \"{}\" has been invalidated", schema->name);
    SGL_CHECK(
        schema->source_layout->owner() == module,
        "Structural ray-tracing program schema \"{}\" does not belong to module \"{}\"",
        schema->name,
        module->name()
    );

    validate_physical_records(options.hit_group_types, options.hit_group_record_data, "hit-group");
    validate_physical_records(options.miss_shader_types, options.miss_shader_record_data, "miss-shader");
    validate_physical_records(options.callable_shader_types, options.callable_shader_record_data, "callable-shader");

    StructuralRayTracingBindings result;
    result.max_ray_payload_size = schema->max_native_payload_size();
    result.max_attribute_size = schema->max_native_hit_attribute_size;
    const bool is_metal = module->session()->device()->info().type == DeviceType::metal;

    // slang-rhi resolves entry points and hit groups through one name map. Reserve every reflected
    // stage export before generating hit-group names so a legal user stage such as
    // `__sgl_structural_hit_group_0` cannot alias a generated name for a physical record selecting
    // another schema function. `raygen_main` is also reserved for SlangPy's generated ray-generation
    // entry point.
    std::set<std::string> used_pipeline_names{"raygen_main"};
    for (const auto& entry_point : module->entry_points())
        used_pipeline_names.insert(entry_point->name());
    auto reserve_stage_name = [&](const std::optional<TraceProgramStageInfo>& stage)
    {
        if (stage && !stage->entry_point_name.empty())
            used_pipeline_names.insert(stage->entry_point_name);
    };
    for (const auto& payload : schema->payloads) {
        for (const auto& group : payload.hit_groups) {
            if (!group.closest_hit_entry_point_name.empty())
                used_pipeline_names.insert(group.closest_hit_entry_point_name);
            reserve_stage_name(group.closest_hit);
            reserve_stage_name(group.any_hit);
            reserve_stage_name(group.intersection);
        }
        for (const auto& shader : payload.miss_shaders)
            reserve_stage_name(shader.miss);
    }
    for (const auto& shader : schema->callable_shaders)
        reserve_stage_name(shader.callable);

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

    std::map<std::string, const TraceProgramHitGroupInfo*> hit_group_by_type;
    std::map<std::string, std::string> native_hit_group_name_by_type;
    std::map<std::string, const TraceProgramMissShaderInfo*> miss_shader_by_type;
    std::map<std::string, std::string> native_miss_name_by_type;
    for (size_t payload_index = 0; payload_index < schema->payloads.size(); ++payload_index) {
        const auto& payload = schema->payloads[payload_index];
        for (const auto& group : payload.hit_groups) {
            SGL_CHECK(
                group.function_index >= 0,
                "Structural ray-tracing hit group \"{}\" has no schema function index",
                group.type_name
            );
            SGL_CHECK(
                hit_group_by_type.emplace(group.type_name, &group).second,
                "Structural ray-tracing schema \"{}\" contains duplicate hit-group type \"{}\"",
                schema->name,
                group.type_name
            );

            std::string hit_group_name_base
                = fmt::format("__sgl_structural_hit_group_p{}_f{}", payload_index, group.function_index);
            std::string hit_group_name = hit_group_name_base;
            uint32_t collision_index = 0;
            while (!used_pipeline_names.insert(hit_group_name).second) {
                ++collision_index;
                hit_group_name = fmt::format("{}_{}", hit_group_name_base, collision_index);
            }

            HitGroupDesc desc;
            desc.hit_group_name = hit_group_name;
            if (group.closest_hit)
                desc.closest_hit_entry_point = resolve_stage(*group.closest_hit, ShaderStage::closest_hit);
            else if (!group.closest_hit_entry_point_name.empty())
                desc.closest_hit_entry_point = group.closest_hit_entry_point_name;
            if (group.any_hit) {
                // Metal folds logical candidate stages into compiler-generated dispatchers reached
                // from structural raygen, so their standalone target entry-point names are empty.
                SGL_CHECK(
                    is_metal || !group.any_hit->entry_point_name.empty(),
                    "Structural ray-tracing any-hit type \"{}\" has no reflected native entry-point name on a "
                    "non-Metal target",
                    group.any_hit->type_name
                );
                if (!group.any_hit->entry_point_name.empty())
                    desc.any_hit_entry_point = resolve_stage(*group.any_hit, ShaderStage::any_hit);
            }
            if (group.intersection) {
                SGL_CHECK(
                    is_metal || !group.intersection->entry_point_name.empty(),
                    "Structural ray-tracing intersection type \"{}\" has no reflected native entry-point name on "
                    "a non-Metal target",
                    group.intersection->type_name
                );
                if (!group.intersection->entry_point_name.empty())
                    desc.intersection_entry_point = resolve_stage(*group.intersection, ShaderStage::intersection);
            }
            native_hit_group_name_by_type.emplace(group.type_name, hit_group_name);
            result.hit_groups.push_back(std::move(desc));
        }

        for (const auto& shader : payload.miss_shaders) {
            SGL_CHECK(
                shader.function_index >= 0,
                "Structural ray-tracing miss shader \"{}\" has no schema function index",
                shader.type_name
            );
            SGL_CHECK(
                miss_shader_by_type.emplace(shader.type_name, &shader).second,
                "Structural ray-tracing schema \"{}\" contains duplicate miss-shader type \"{}\"",
                schema->name,
                shader.type_name
            );
            SGL_CHECK(shader.miss, "Structural ray-tracing miss shader \"{}\" has no miss stage", shader.type_name);
            native_miss_name_by_type.emplace(shader.type_name, resolve_stage(*shader.miss, ShaderStage::miss));
        }
    }

    std::map<std::string, const TraceProgramCallableShaderInfo*> callable_shader_by_type;
    std::map<std::string, std::string> native_callable_name_by_type;
    for (const auto& shader : schema->callable_shaders) {
        SGL_CHECK(
            shader.function_index >= 0,
            "Structural ray-tracing callable shader \"{}\" has no schema function index",
            shader.type_name
        );
        SGL_CHECK(
            callable_shader_by_type.emplace(shader.type_name, &shader).second,
            "Structural ray-tracing schema \"{}\" contains duplicate callable-shader type \"{}\"",
            schema->name,
            shader.type_name
        );
        SGL_CHECK(
            shader.callable,
            "Structural ray-tracing callable shader \"{}\" has no callable stage",
            shader.type_name
        );
        native_callable_name_by_type.emplace(shader.type_name, resolve_stage(*shader.callable, ShaderStage::callable));
    }

    auto normalize_data = [](const std::vector<std::vector<uint8_t>>& input,
                             size_t index,
                             size_t expected_size,
                             std::string_view kind,
                             std::string_view type_name)
    {
        std::vector<uint8_t> data = input.empty() ? std::vector<uint8_t>() : input[index];
        if (data.empty() && expected_size > 0)
            data.resize(expected_size, 0);
        SGL_CHECK(
            data.size() == expected_size,
            "Structural ray-tracing {} record for \"{}\" has {} application bytes; the reflected record layout "
            "requires {}",
            kind,
            type_name,
            data.size(),
            expected_size
        );
        return data;
    };

    bool needs_empty_hit_group = false;
    result.hit_group_names.reserve(options.hit_group_types.size());
    result.hit_group_record_data.reserve(options.hit_group_types.size());
    for (size_t index = 0; index < options.hit_group_types.size(); ++index) {
        const auto& type_name = options.hit_group_types[index];
        if (type_name.empty()) {
            SGL_CHECK(
                options.hit_group_record_data.empty() || options.hit_group_record_data[index].empty(),
                "Empty structural hit-group record {} cannot carry application data",
                index
            );
            needs_empty_hit_group = true;
            result.hit_group_names.emplace_back();
            result.hit_group_record_data.emplace_back();
            continue;
        }
        auto found = hit_group_by_type.find(type_name);
        SGL_CHECK(
            found != hit_group_by_type.end(),
            "Structural ray-tracing hit-group type \"{}\" is not a member of schema \"{}\"",
            type_name,
            schema->name
        );
        const auto* group = found->second;
        result.hit_group_names.push_back(native_hit_group_name_by_type.at(type_name));
        result.hit_group_record_data.push_back(normalize_data(
            options.hit_group_record_data,
            index,
            reflected_record_size(group->record_type, group->record_type_layout, "hit-group", type_name),
            "hit-group",
            type_name
        ));
    }

    if (needs_empty_hit_group) {
        std::string dummy_name_base = "__sgl_structural_empty_hit_group";
        std::string dummy_name = dummy_name_base;
        uint32_t collision_index = 0;
        while (!used_pipeline_names.insert(dummy_name).second) {
            ++collision_index;
            dummy_name = fmt::format("{}_{}", dummy_name_base, collision_index);
        }
        result.hit_groups.emplace_back(dummy_name);
        for (auto& name : result.hit_group_names) {
            if (name.empty())
                name = dummy_name;
        }
    }

    result.miss_entry_points.reserve(options.miss_shader_types.size());
    result.miss_shader_record_data.reserve(options.miss_shader_types.size());
    for (size_t index = 0; index < options.miss_shader_types.size(); ++index) {
        const auto& type_name = options.miss_shader_types[index];
        if (type_name.empty()) {
            SGL_CHECK(
                options.miss_shader_record_data.empty() || options.miss_shader_record_data[index].empty(),
                "Empty structural miss-shader record {} cannot carry application data",
                index
            );
            result.miss_entry_points.emplace_back();
            result.miss_shader_record_data.emplace_back();
            continue;
        }
        auto found = miss_shader_by_type.find(type_name);
        SGL_CHECK(
            found != miss_shader_by_type.end(),
            "Structural ray-tracing miss-shader type \"{}\" is not a member of schema \"{}\"",
            type_name,
            schema->name
        );
        const auto* shader = found->second;
        result.miss_entry_points.push_back(native_miss_name_by_type.at(type_name));
        result.miss_shader_record_data.push_back(normalize_data(
            options.miss_shader_record_data,
            index,
            reflected_record_size(shader->record_type, shader->record_type_layout, "miss-shader", type_name),
            "miss-shader",
            type_name
        ));
    }

    result.callable_entry_points.reserve(options.callable_shader_types.size());
    result.callable_shader_record_data.reserve(options.callable_shader_types.size());
    for (size_t index = 0; index < options.callable_shader_types.size(); ++index) {
        const auto& type_name = options.callable_shader_types[index];
        if (type_name.empty()) {
            SGL_CHECK(
                options.callable_shader_record_data.empty() || options.callable_shader_record_data[index].empty(),
                "Empty structural callable-shader record {} cannot carry application data",
                index
            );
            result.callable_entry_points.emplace_back();
            result.callable_shader_record_data.emplace_back();
            continue;
        }
        auto found = callable_shader_by_type.find(type_name);
        SGL_CHECK(
            found != callable_shader_by_type.end(),
            "Structural ray-tracing callable-shader type \"{}\" is not a member of schema \"{}\"",
            type_name,
            schema->name
        );
        const auto* shader = found->second;
        result.callable_entry_points.push_back(native_callable_name_by_type.at(type_name));
        result.callable_shader_record_data.push_back(normalize_data(
            options.callable_shader_record_data,
            index,
            reflected_record_size(shader->record_type, shader->record_type_layout, "callable-shader", type_name),
            "callable-shader",
            type_name
        ));
    }

    return result;
}

StructuralRayTracingBindings create_structural_ray_tracing_bindings(
    const SlangModule* module,
    std::string_view schema_name,
    const StructuralRayTracingBindingOptions& options
)
{
    SGL_CHECK(module, "Structural ray-tracing binding creation requires a Slang module");
    ref<const TraceProgramSchemaInfo> schema = module->layout()->find_trace_program_schema(schema_name);
    SGL_CHECK(
        schema,
        "Structural ray-tracing program schema \"{}\" was not found in module \"{}\"",
        schema_name,
        module->name()
    );
    return create_structural_ray_tracing_bindings(module, schema.get(), options);
}

ShaderTable::ShaderTable(ref<Device> device, ShaderTableDesc desc)
    : DeviceChild(std::move(device))
{
    auto make_rhi_record_data
        = [](const std::vector<std::vector<uint8_t>>& records, size_t expected_count, std::string_view kind)
    {
        SGL_CHECK(
            records.empty() || records.size() == expected_count,
            "Shader-table {} record-data count {} must be zero or match the shader record count {}",
            kind,
            records.size(),
            expected_count
        );
        std::vector<rhi::ShaderRecordData> result;
        if (records.empty())
            return result;
        result.reserve(records.size());
        for (const auto& record : records) {
            result.push_back({
                .data = record.empty() ? nullptr : record.data(),
                .size = record.size(),
            });
        }
        return result;
    };

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

    auto rhi_miss_record_data
        = make_rhi_record_data(desc.miss_shader_record_data, desc.miss_entry_points.size(), "miss-shader");
    auto rhi_hit_group_record_data
        = make_rhi_record_data(desc.hit_group_record_data, desc.hit_group_names.size(), "hit-group");
    auto rhi_callable_record_data
        = make_rhi_record_data(desc.callable_shader_record_data, desc.callable_entry_points.size(), "callable-shader");

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
        .missShaderRecordData = rhi_miss_record_data.empty() ? nullptr : rhi_miss_record_data.data(),
        .hitGroupRecordData = rhi_hit_group_record_data.empty() ? nullptr : rhi_hit_group_record_data.data(),
        .callableShaderRecordData = rhi_callable_record_data.empty() ? nullptr : rhi_callable_record_data.data(),
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
