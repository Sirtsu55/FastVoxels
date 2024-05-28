#include "Shaders/Common/Common.hlsl"

#define EPSILON 0.01

[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;
    attribs.Normal = 0;
    
    AABB aabb = aabbBuffer[PrimitiveIndex()];
    
    float3 boxMid = (aabb.Min + aabb.Max) / 2;
    
    boxMid = mul(ObjectToWorld4x3(), float4(boxMid, 1.0)).xyz;
    
    float t = distance(boxMid, WorldRayOrigin());

    ReportHit(t, 0, attribs);
}