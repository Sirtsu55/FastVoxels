#include "Shaders/Common/Common.hlsl"

[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;
    attribs.Normal = 0;

    AABB aabb = aabbBuffer[PrimitiveIndex()];

    float3 middle = aabb.Min + ((aabb.Max - aabb.Min) / 2.0);

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
        float3 tmin = min(t0,t1), tmax = max(t0,t1);
    
        float mn =  max(max(tmin.x, tmin.y), tmin.z);
        float mx = min(min(tmax.x, tmax.y), tmax.z);

        if (mn > mx)
        {
            return;
        }

        RayT = mn;

        hitLoc = rayOrigin + rayDir * RayT;
    }

    // Calculate the normal
    {
        float3 midToHit = hitLoc - middle;

        float absX = abs(midToHit.x);
        float absY = abs(midToHit.y);
        float absZ = abs(midToHit.z);

            if (absX > absY && absX > absZ)
            {
                attribs.Normal.x = sign(midToHit.x);
            }
            else if (absY > absX && absY > absZ)
            {
                attribs.Normal.y = sign(midToHit.y);
            }
            else if (absZ > absX && absZ > absY)
            {
                attribs.Normal.z = sign(midToHit.z);
            }
    }

    ReportHit(RayT, 0, attribs);
    
}