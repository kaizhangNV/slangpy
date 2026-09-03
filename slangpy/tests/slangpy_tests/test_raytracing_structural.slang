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

struct TestTraceContext : rt::ITraceContext
{
    typealias Payload = ::Payload;
    typealias AccelerationStructure = rt::AccelerationStructure;
    typealias Motion = rt::NoMotion;
}

struct TestHitContext : rt::IHitContext
{
    typealias TraceContext = TestTraceContext;
    typealias Primitive = rt::TrianglePrimitive;
    typealias Record = void;
}

namespace StructuralCanaryStages {
struct TestClosestHit : rt::IClosestHitShader<::TestHitContext>
{
    void invoke(rt::ClosestHitInput<::TestHitContext> input)
    {
        IStructuralHitColor hitColor = createDynamicObject<IStructuralHitColor>(0, 0);
        input.payload.color = hitColor.evaluate(input.triangle.barycentricCoord);
    }
}

struct TestHitGroup : rt::IHitGroup
{
    typealias Slot = rt::HitGroupSlot<0>;
    typealias Context = ::TestHitContext;
    typealias ClosestHit = TestClosestHit;
    typealias AnyHit = rt::NoAnyHit<::TestHitContext>;
    typealias Intersection = rt::NoIntersection<::TestHitContext>;
}

struct TestMissContext : rt::IMissGroupContext
{
    typealias TraceContext = ::TestTraceContext;
    typealias Record = void;
}

struct TestMiss : rt::IMissShader<TestMissContext>
{
    void invoke(rt::MissInput<TestMissContext> input) { input.payload.color = float3(1, 0, 1); }
}

struct TestMissGroup : rt::IMissGroup
{
    typealias Slot = rt::MissSlot<0>;
    typealias Context = TestMissContext;
    typealias Miss = TestMiss;
}
} // namespace StructuralCanaryStages

struct TestProgramLayout : rt::ITraceProgramLayout
{
    typealias TraceContext = TestTraceContext;
    typealias HitGroups = rt::HitGroupList<TestTraceContext, StructuralCanaryStages::TestHitGroup>;
    typealias MissGroups = rt::MissGroupList<TestTraceContext, StructuralCanaryStages::TestMissGroup>;
    typealias CallableGroups = rt::NoCallableGroups<TestTraceContext>;
}

rt::TraceProgramDescriptor<TestProgramLayout> testProgram;

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
    desc.sbtOffset = 0;
    desc.sbtStride = 0;
    desc.missIndex = 0;

    Payload payload = {};
    rt::RayTracer<TestProgramLayout> tracer;
    tracer.trace(desc, tlas, testProgram, payload);
    return payload.color;
}
