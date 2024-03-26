#include "Shaders/Ray.hlsl"
#include "Shaders/Random.hlsl"
#include "Shaders/Sampling.hlsl"
#include "Shaders/Structs.hlsl"

struct AABB
{
    float3 Min;
    float3 Max;
};

[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]]
cbuffer uniformBuffer
{
    float4x4 viewInverse;
    float4x4 projInverse;
    float4 otherInfo;
};
[[vk::binding(2, 0)]] RWTexture2D<float4> image;
[[vk::binding(3, 0)]] StructuredBuffer<AABB> aabbBuffer;


[shader("raygeneration")]
void rgen()
{
    uint3 LaunchID = DispatchRaysIndex();
    uint3 LaunchSize = DispatchRaysDimensions();

    RayDesc rayDesc = ConstructRay(viewInverse, projInverse);

    Payload payload;

    float3 Radiance = float3(1.0, 1.0, 1.0);

    TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);

    image[int2(LaunchID.xy)] = float4(payload.HitColor, 0.0);
}


[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;

    AABB aabb = aabbBuffer[PrimitiveIndex()];

    // Calculate the middle point of the AABB

    float3 middle = aabb.Min + ((aabb.Min - aabb.Max) / 2.0);

    attribs.Normal = dot(middle, ObjectRayDirection());

    ReportHit(RayTCurrent(), 0, attribs);
    
}


[shader("miss")]
void miss(inout Payload p)
{
    // RT in one weekend sky
    float3 unitDirection = normalize(WorldRayDirection());
    float t = 0.5 * (unitDirection.y + 1.0);
    p.HitColor = (1.0 - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
}