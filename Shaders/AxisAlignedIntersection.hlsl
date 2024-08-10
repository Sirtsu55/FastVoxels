#include "Shaders/Common/Common.hlsl"

static const uint SceneConstantsIndex = 0;
static const uint OutputBufferIndex = 1;
static const uint AccumulationBufferIndex = 2;
static const uint ColorBufferIndex = 3;
static const uint AABBBufferIndexStart = 4;

struct VoxMaterial
{
    uint Color;
    float Emission;
};

AABB GetAABB()
{
    StructuredBuffer<AABB> buf = ResourceDescriptorHeap[AABBBufferIndexStart + InstanceID()];
    
    return buf[PrimitiveIndex()];

}

VoxMaterial GetColor(uint index)
{
    StructuredBuffer<VoxMaterial> buf = ResourceDescriptorHeap[ColorBufferIndex];
    return buf[index];
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

float3 getNormal(float3 pc)
{
    float3 normal = 0.0;
    
    if (abs(pc.x) > abs(pc.y) && abs(pc.x) > abs(pc.z))
        normal.x = sign(pc.x);
    else if (abs(pc.y) > abs(pc.z))
        normal.y = sign(pc.y);
    else
        normal.z = sign(pc.z);
    
    return normalize(normal);
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
    info.Normal = mul((float3x3) ObjectToWorld3x4(), info.Normal);
    
    return info;
}


[shader("raygeneration")]
void rgen()
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[SceneConstantsIndex];
    RWTexture2D<float4> outImage = ResourceDescriptorHeap[OutputBufferIndex];
    RWTexture2D<float4> accumImage = ResourceDescriptorHeap[AccumulationBufferIndex];
   
    
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    uint seed = asuint(LaunchID.x) * asuint(LaunchID.y) * asuint(sceneInfo.otherInfo.y);
    RayDesc rayDesc = ConstructRay(sceneInfo.View, sceneInfo.Proj, seed);

    Payload p;
    p.HitColor = 1;
    
    for (uint i = 0; i < 4; i++)
    {
        TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, p);
        if(p.T == -1.0f)
            break;
        
        rayDesc.Origin = rayDesc.Origin + p.T * rayDesc.Direction;
        rayDesc.Direction = normalize(p.RayDirection);
    }
    
    const int2 index = int2(LaunchID.xy);
    float3 Radiance = p.HitColor * p.Emission;
    
    if (any(isnan(Radiance)) || any(isinf(Radiance)))
        Radiance = float3(0.0, 0.0, 0.0);
    
    uint frameCount = asuint(sceneInfo.otherInfo.x);
    float4 accum = accumImage[index];
    accum.rgb = frameCount == 0 ? Radiance : accum.rgb + Radiance;
    
    accumImage[index] = accum;
    outImage[index] = accum / float(frameCount + 1);
}

[shader("intersection")]
void isect()
{
    AABB voxel = GetAABB();
    
    // Calculate the distance to the voxel
    float3 voxelMid = (voxel.Max + voxel.Min) / 2.0;
    
    float dist = distance(ObjectRayOrigin(), voxelMid);
    
    ReportHit(dist, 0, voxel);
}


[shader("closesthit")]
void chit(inout Payload p, in AABB voxel)
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[SceneConstantsIndex];
    
    const float SceneEmissiveIntensity = asfloat(sceneInfo.otherInfo.z);
    
    HitInfo info = getHitInfo(voxel);
    if (info.T == -1.0f)
    {
        p.HitColor = float3(0.0, 0.0, 0.0);
        p.T = -1.0f;
        return;
    }
    
    float3 v = ObjectRayDirection();
    float time = asfloat(sceneInfo.otherInfo.y);
    uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z);
    
    float2 rand = float2(NextRandomFloat(seed), NextRandomFloat(seed));
    
    VoxMaterial m = GetColor(voxel.ColorIndex);
    float3 Color = float3((m.Color & 0xFF) / 255.0, ((m.Color >> 8) & 0xFF) / 255.0, ((m.Color >> 16) & 0xFF) / 255.0);
    p.HitColor *= Color;
    p.RayDirection = SampleCosineHemisphere(info.Normal, rand);
    p.Emission = m.Emission * SceneEmissiveIntensity;
    if (m.Emission > 0)
    {
        p.T = -1.0f;
    }
    else
    {
        p.T = info.T;
    }
    
    //p.HitColor = info.Normal;
    //p.Emission = 1;
    //p.T = -1.0f;
    
}

[shader("miss")]
void miss(inout Payload p)
{
    p.HitColor *= float3(0.0, 0.0, 0.0);
    p.Emission = 1.0;
    p.T = -1.0f;
    
    //p.HitColor = float3(0.0, 0.0, 0.0);
    //p.Emission = 0.0;
    //p.T = -1.0f;
    
}