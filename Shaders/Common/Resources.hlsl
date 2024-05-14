
struct AABB
{
    float3 Min;
    float3 Max;
};

struct BoxHitAttributes
{
    float3 Normal;
};

struct Payload
{
    float3 HitColor;
    
    float HitLight;
    float3 HitLoc;
    
    float3 NextDir;
    bool TerminateRay;
};

RaytracingAccelerationStructure rs;

struct SceneInfo
{
    float4x4 View;
    float4x4 Proj;
    float4 otherInfo;
};

StructuredBuffer<AABB> aabbBuffer;
RWTexture2D<float4> outImage;
RWTexture2D<float4> accumImage;

ProceduralPrimitiveHitGroup AABBHitGroup =
{
    "", // AnyHit
    "chit", // ClosestHit
    "isect", // Intersection
};

RaytracingShaderConfig ShaderConfig =
{
    44,
    16
};

RaytracingPipelineConfig PipelineConfig =
{
    1
};

