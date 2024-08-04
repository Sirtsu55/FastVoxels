#include "Shaders/Common/Common.hlsl"

#define MAX_BOUNCES 2

[shader("raygeneration")]
void rgen()
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[0];
    RWTexture2D<float4> outImage = ResourceDescriptorHeap[1];
    RWTexture2D<float4> accumImage = ResourceDescriptorHeap[2];
    
    
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    RayDesc rayDesc = ConstructRay(sceneInfo.View, sceneInfo.Proj);

    Payload payload;
    payload.HitColor = float3(1.0, 1.0, 1.0);

    float3 Radiance = 1.0;

    for (int i = 0; i < MAX_BOUNCES; i++)
    {
        TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);
        
        if (payload.TerminateRay)   
            break;

        rayDesc.Origin = payload.HitLoc;
        rayDesc.Direction = payload.NextDir;
    }

    Radiance = payload.HitColor * payload.HitLight;
    
    if (any(isnan(Radiance)))
        Radiance = float3(0.0, 0.0, 0.0);
    
    uint frameCount = asuint(sceneInfo.otherInfo.x);
    
    float3 Accumulated = 0;
    
    if (frameCount == 0)
        Accumulated = Radiance;
    else
        Accumulated = accumImage[int2(LaunchID.xy)].xyz + Radiance;
    
    accumImage[int2(LaunchID.xy)] = float4(Accumulated, 0.0);

    float3 FinalColor = (Accumulated) / float(1 + frameCount);
    
    outImage[int2(LaunchID.xy)] = float4(FinalColor, 0.0);
}

float max_component(float3 v)
{
    return max(max(v.x, v.y), v.z);
}

float min_component(float3 v)
{
    return min(min(v.x, v.y), v.z);
}

float slabs(float3 p0, float3 p1, float3 rayOrigin, float3 invRaydir)
{
    float3 t0 = (p0 - rayOrigin) * invRaydir;
    float3 t1 = (p1 - rayOrigin) * invRaydir;
    float3 tmin = min(t0, t1), tmax = max(t0, t1);
  
    float min = max_component(tmin);
    float max = min_component(tmax);
    
    return min <= max ? min : -1.0f;
}


[shader("intersection")]
void isect()
{
    StructuredBuffer<AABB> aabbBuffer = ResourceDescriptorHeap[3];
    AABB voxel = aabbBuffer[PrimitiveIndex()];
    
    // Calculate the distance to the voxel
    float3 voxelMid = (voxel.Max + voxel.Min) / 2.0;
    float dist = distance(ObjectRayOrigin(), voxelMid);
    
    ReportHit(dist, 0, voxel);
}

float3 getNormal(float3 hitPointLocal)
{
    if (abs(hitPointLocal.x) > abs(hitPointLocal.y) && abs(hitPointLocal.x) > abs(hitPointLocal.z))
        return float3(sign(hitPointLocal.x), 0.0, 0.0);
    else if (abs(hitPointLocal.y) > abs(hitPointLocal.x) && abs(hitPointLocal.y) > abs(hitPointLocal.z))
        return float3(0.0, sign(hitPointLocal.y), 0.0);
    else
        return float3(0.0, 0.0, sign(hitPointLocal.z));
}

HitInfo getHitInfo(in AABB voxel)
{
    HitInfo info;
    
    info.T = slabs(voxel.Min, voxel.Max, ObjectRayOrigin(), rcp(ObjectRayDirection()));
    
    if (info.T == -1.0f)
    {
        info.Normal = float3(0.0, 0.0, 0.0);
        return info;
    }
    
    // Calculate the normal
    float3 voxelMid = (voxel.Max + voxel.Min) / 2.0;
    info.Normal = getNormal((ObjectRayOrigin() + info.T * ObjectRayDirection()) - voxelMid);
    
    return info;
}

[shader("closesthit")]
void chit(inout Payload p, in AABB voxel)
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[0];
    
    HitInfo info = getHitInfo(voxel);
    
    if (info.T == -1.0f)
    {
        p.TerminateRay = true;
        return;
    }
    
    float3 v = WorldRayDirection();
    float time = asfloat(sceneInfo.otherInfo.y);
    uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z);

	// Sample a new ray direction
    float2 rand = float2(NextRandomFloat(seed), NextRandomFloat(seed));
    
    uint color = Random(PrimitiveIndex());
    
    float3 Color = float3((color & 0xFF) / 255.0, ((color >> 8) & 0xFF) / 255.0, ((color >> 16) & 0xFF) / 255.0);
    
    p.HitColor *= Color;
    p.HitLight = 0.0;
    p.TerminateRay = false;
    p.NextDir = SampleCosineHemisphere(info.Normal, rand);
    p.HitLoc = ObjectRayOrigin() + info.T * ObjectRayDirection();
}

[shader("miss")]
void miss(inout Payload p)
{
    // RT in one weekend sky
    float3 unitDirection = normalize(WorldRayDirection());
    float t = 0.5 * (unitDirection.y + 1.0);
    p.HitColor *= lerp(float3(1.0, 1.0, 1.0), float3(0.5, 0.7, 1.0), t);
    p.HitLight = 1.0f;
    p.TerminateRay = true;
}