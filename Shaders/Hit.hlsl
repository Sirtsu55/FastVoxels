#include "Shaders/Structs.hlsl"

[shader("closesthit")]
void chit(inout Payload p, in BoxHitAttributes attribs)
{
    p.HitColor = attribs.Normal;
}