#include "Shaders/Common/Common.hlsl"



[shader("intersection")]
void isect()
{
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
    
}
