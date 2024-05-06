#include "Shaders/Common/Common.hlsl"

#define EPSILON 0.01

[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;
    attribs.Normal = 0;

    AABB aabb = aabbBuffer[PrimitiveIndex()];

    float3 middle = aabb.Min + ((aabb.Max - aabb.Min) / 2.0);
    float3 invMid = 1.0 / middle;

    float3 hitLoc;

    float RayT = -1.0;
    

    // Calculate the middle point of the AABB
    {
        const float3 p0 = aabb.Min;
        const float3 p1 = aabb.Max;

        const float3 rayOrigin = ObjectRayOrigin();
        const float3 rayDir = ObjectRayDirection();
        const float3 invRaydir = 1.0 / rayDir;

        float3 t0 = (p0 - rayOrigin) * invRaydir;
        float3 t1 = (p1 - rayOrigin) * invRaydir;
        float3 tmin = min(t0, t1), tmax = max(t0, t1);
    
        float mn = max(max(tmin.x, tmin.y), tmin.z);
        float mx = min(min(tmax.x, tmax.y), tmax.z);

        if (mn <= mx)
            RayT = mn;
        
        hitLoc = rayOrigin + rayDir * RayT;
    }

    if (RayT > 0.0)
    {
        const float3 center = 0.5 * (aabb.Min + aabb.Max);
        const float3 centerToPoint = hitLoc - center;
        const float3 halfSize = 0.5 * (aabb.Max - aabb.Min);
        attribs.Normal = normalize(sign(centerToPoint) * step(-EPSILON, abs(centerToPoint) - halfSize));
    }
    else
    {
        attribs.Normal = 1;
    }
    
    
    ReportHit(RayT, 0, attribs);

    
}