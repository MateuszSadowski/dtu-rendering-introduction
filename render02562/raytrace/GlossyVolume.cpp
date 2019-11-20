// 02562 Rendering Framework
// Written by Jeppe Revall Frisvad, 2011
// Copyright (c) DTU Informatics 2011

#include <optix_world.h>
#include "HitInfo.h"
#include "int_pow.h"
#include "GlossyVolume.h"

using namespace optix;

#ifndef M_1_PIf
#define M_1_PIf 0.31830988618379067154
#endif

float3 GlossyVolume::shade(const Ray& r, HitInfo& hit, bool emit) const
{
  // Compute the specular part of the glossy shader and attenuate it
  // by the transmittance of the material if the ray is inside (as in
  // the volume shader).

  // copy things from Phong shade
  // copy things from Glossy shade
  // copy things from Absorption
  // do the check again and multiply by transmittance


    if(dot(hit.shading_normal, r.direction)) {
        return get_transmittance(hit) * Transparent::shade(r, hit, emit);
    }

  return Volume::shade(r, hit, emit);
}
