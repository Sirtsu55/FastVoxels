#include "Shaders/Common/Common.hlsl"

static const uint ColorBufferIndex = 3;
static const uint AABBBufferIndex = 4;
static const uint DistanceFieldTextureIndexStart = 5;

#define EPSILON 0.0001f

struct VoxMaterial
{
    uint Color;
    float Emission;
};

struct AABB
{
    float3 Min;
    float3 Max;
};

struct Attribute
{
    uint3 Nothing;
};


AABB GetAABB()
{
    StructuredBuffer<AABB> buf = ResourceDescriptorHeap[AABBBufferIndex];
    
    return buf[InstanceID()];
}


Texture3D<uint> GetDistanceField()
{
    return ResourceDescriptorHeap[NonUniformResourceIndex(DistanceFieldTextureIndexStart + InstanceID())];
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

[shader("intersection")]
void isect()
{
    AABB aabb = GetAABB();
    
    float T = slabs(aabb.Min, aabb.Max, ObjectRayOrigin(), rcp(ObjectRayDirection()));
    if(T == -1.0f)
        return;
    
    Attribute attr;
    ReportHit(T, 0, attr);
}

[shader("closesthit")]
void chit(inout Payload p, in Attribute attr)
{
    float3 hitLoc = ObjectRayOrigin() + RayTCurrent() * ObjectRayDirection() - EPSILON;
    uint3 iHitLoc = uint3(hitLoc);
    
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[SceneConstantsIndex];
    const float SceneEmissiveIntensity = asfloat(sceneInfo.otherInfo.z);
    
    float3 v = ObjectRayDirection();
    float time = asfloat(sceneInfo.otherInfo.y);
    uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z);
    float2 rand = float2(NextRandomFloat(seed), NextRandomFloat(seed));
    
    Texture3D<uint> distanceField = GetDistanceField();
    uint voxInfo = distanceField.Load(int4(iHitLoc, 0));
    uint distance = voxInfo & 0x0000000F;
    uint materialIndex = (voxInfo & 0xFFFFFFF0) >> 4;
    
    float3 voxMid = iHitLoc + 0.5f;
    float3 normal = getNormal(hitLoc - voxMid);
    
    // Rotate normal
    normal = mul((float3x3) ObjectToWorld3x4(), normal);
    
    VoxMaterial m = GetColor(materialIndex);
    
    const float3 color = float3((m.Color & 0xFF) / 255.0, ((m.Color >> 8) & 0xFF) / 255.0, ((m.Color >> 16) & 0xFF) / 255.0);
    p.HitColor *= color;
    p.RayDirection = SampleCosineHemisphere(normal, rand);
    
    // If there is emission, we don't need to trace further.
    if (m.Emission > 0.0)
    {
        p.Emission = m.Emission * SceneEmissiveIntensity;
        p.T = -1.0f;
    }
    else
    {
        p.Emission = 0.0;
        p.T = RayTCurrent();
    }
}
