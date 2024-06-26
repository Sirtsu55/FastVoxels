
struct AABB
{
    float3 Min;
    float3 Max;
};

struct BoxHitAttributes
{
    float3 Normal;
};

struct [raypayload] Payload
{
    [[vk::location(0)]] float3 HitColor;
    [[vk::location(1)]] float HitLight;
    [[vk::location(3)]] float3 NextOrigin;
    [[vk::location(4)]] float3 NextDir;
    [[vk::location(5)]] bool TerminateRay;

};


[[vk::binding(0, 0)]] RaytracingAccelerationStructure rs;
[[vk::binding(1, 0)]]
cbuffer uniformBuffer
{
    float4x4 viewInverse;
    float4x4 projInverse;
    float4 otherInfo;
};
[[vk::binding(2, 0)]] StructuredBuffer<AABB> aabbBuffer;
[[vk::binding(3, 0)]] RWTexture2D<float4> outImage;
[[vk::binding(4, 0)]] RWTexture2D<float4> accumImage;

