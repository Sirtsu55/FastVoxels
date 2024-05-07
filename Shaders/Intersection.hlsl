#include "Shaders/Common/Common.hlsl"

#define EPSILON 0.01

[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;
    attribs.Normal = 0;

    ReportHit(1, 0, attribs);
}