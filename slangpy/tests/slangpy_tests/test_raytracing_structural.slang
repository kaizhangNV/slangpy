// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
import "slangpy";
import slang.raytracing;

interface IStructuralHitColor
{
    float3 evaluate(float2 barycentrics);
}

struct BarycentricHitColor : IStructuralHitColor
{
    uint dummy;

    float3 evaluate(float2 barycentrics) { return float3(barycentrics, 0); }
}

struct Payload
{
    float3 color;
}

struct TestRecord
{
    float3 color;
}

struct TestTraceContext : rt::ITraceContext
{
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}

struct TestHitContext : rt::IHitContext
{
    typealias TraceContext = TestTraceContext;
    typealias Payload = ::Payload;
    typealias Primitive = rt::TrianglePrimitive;
    typealias Record = ::TestRecord;
}

namespace StructuralCanaryStages {
struct TestClosestHit : rt::IClosestHitShader
{
    typealias Context = ::TestHitContext;

    void invoke(rt::ClosestHitInput<::TestHitContext> input)
    {
        IStructuralHitColor hitColor = createDynamicObject<IStructuralHitColor>(0, 0);
        input.payload.color = hitColor.evaluate(input.triangle.barycentricCoord) + input.record.color;
    }
}

struct TestHitGroup : rt::IHitGroup
{
    typealias Context = ::TestHitContext;
    typealias ClosestHit = TestClosestHit;
    typealias AnyHit = rt::NoAnyHit<::TestHitContext>;
    typealias Intersection = rt::NoIntersection<::TestHitContext>;
}

struct TestMissContext : rt::IPayloadContext
{
    typealias TraceContext = ::TestTraceContext;
    typealias Payload = ::Payload;
    typealias Record = ::TestRecord;
}

struct TestMiss : rt::IMissShader
{
    typealias Context = TestMissContext;

    void invoke(rt::MissInput<TestMissContext> input) { input.payload.color = input.record.color; }
}
} // namespace StructuralCanaryStages

struct TestProgramSchema : rt::ITraceProgramSchema
{
    typealias TraceContext = TestTraceContext;
    typealias HitGroups = rt::HitGroupList<StructuralCanaryStages::TestHitGroup>;
    typealias MissShaders = rt::MissShaderList<StructuralCanaryStages::TestMiss>;
    typealias CallableShaders = rt::NoCallableShaders;
}

rt::TraceProgramDescriptor<TestProgramSchema> testProgram;

float3 trace(int2 tid, RaytracingAccelerationStructure tlas)
{
    float2 uv = (float2(tid) + 0.5) / 64.0;

    rt::RayTraversalDesc desc = {};
    desc.ray.origin = float3(uv * 2 - 1, 1);
    desc.ray.direction = float3(0, 0, -1);
    desc.ray.tMin = 0;
    desc.ray.tMax = 2;
    desc.rayFlags = RAY_FLAG_NONE;
    desc.instanceMask = 0xff;
    // Select between two physical records that both point at the same schema entry.
    // This exercises the host-owned SBT mapping rather than merely reflecting the schema.
    desc.sbtOffset = tid.x >= 32 ? 1 : 0;
    desc.sbtStride = 0;
    desc.missIndex = tid.x >= 32 ? 1 : 0;

    Payload payload = {};
    rt::RayTracer<TestProgramSchema> tracer;
    tracer.trace(desc, tlas, testProgram, payload);
    return payload.color;
}
