#pragma once

struct AABB
{
    float3 Min;
    float3 Max;
    uint ColorIndex;
    uint Padding;
};

struct HitInfo
{
    float T;
    float3 Normal;
};

struct Payload
{
    float3 HitColor;
    float3 RayDirection;
    float T;
    float Light;
};

struct SceneInfo
{
    float4x4 View;
    float4x4 Proj;
    float4 otherInfo;
};

RaytracingAccelerationStructure rs : register(t0);

ProceduralPrimitiveHitGroup AABBHitGroup =
{
    "", // AnyHit
    "chit", // ClosestHit
    "isect", // Intersection
};

RaytracingShaderConfig ShaderConfig =
{
    44,
    32
};

RaytracingPipelineConfig PipelineConfig =
{
    1
};

GlobalRootSignature RootSig =
{
    "RootFlags( CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED )," // Root flags
    "SRV( t0 ),"                                          // Acceleration structure
};
