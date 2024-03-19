#include "Shaders/Ray.hlsl"
#include "Shaders/Random.hlsl"
#include "Shaders/Sampling.hlsl"

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



struct Payload
{
    [[vk::location(0)]] float3 HitColor;

    [[vk::location(1)]] float3 NextRayDir;
};

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

struct BoxHitAttributes
{
    float3 Normal;
};

[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;

    
    ReportHit(RayTCurrent(), 0, attribs);
}

[shader("closesthit")]
void chit(inout Payload p, in BoxHitAttributes attribs)
{
    p.HitColor = float3(1.0f, 1.0f, 1.0f);

    // Reflect the ray around the normal
    // p.NextRayDir = reflect(WorldRayDirection(), normal);

    // Get the AABB geometry
    AABB aabb = aabbBuffer[PrimitiveIndex()];

    p.HitColor = aabb.Max - aabb.Min;
}


[shader("miss")]
void miss(inout Payload p)
{
    // RT in one weekend sky
    float3 unitDirection = normalize(WorldRayDirection());
    float t = 0.5 * (unitDirection.y + 1.0);
    p.HitColor = (1.0 - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
}