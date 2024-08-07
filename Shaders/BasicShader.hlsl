#include "Shaders/Common/Common.hlsl"

AABB GetAABB()
{
    StructuredBuffer<AABB> buf = ResourceDescriptorHeap[3 + InstanceID()];
    
    return buf[PrimitiveIndex()];

}

uint GetColor(uint index)
{
    ByteAddressBuffer buf = ResourceDescriptorHeap[2];
    return buf.Load(index * 4);
}

[shader("raygeneration")]
void rgen()
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[0];
    RWTexture2D<float4> outImage = ResourceDescriptorHeap[1];
    
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    RayDesc rayDesc = ConstructRay(sceneInfo.View, sceneInfo.Proj);

    Payload payload;
    float3 Radiance = 1.0;

    TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, payload);

    outImage[int2(LaunchID.xy)] = float4(payload.HitColor, 0.0);
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
    AABB voxel = GetAABB();
    
    // Calculate the distance to the voxel
    float3 voxelMid = (voxel.Max + voxel.Min) / 2.0;
    
    float dist = distance(ObjectRayOrigin(), voxelMid);
    
    ReportHit(dist, 0, voxel);
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

[shader("closesthit")]
void chit(inout Payload p, in AABB voxel)
{
    HitInfo info = getHitInfo(voxel);
    if (info.T == -1.0f)
    {
        p.HitColor = float3(0.0, 0.0, 0.0);
        return;
    }
    
    uint color = GetColor(voxel.ColorIndex);
    float3 Color = float3((color & 0xFF) / 255.0, ((color >> 8) & 0xFF) / 255.0, ((color >> 16) & 0xFF) / 255.0);
    p.HitColor = Color;
}

[shader("miss")]
void miss(inout Payload p)
{
    p.HitColor = float3(0.5, 0.7, 1.0);
}