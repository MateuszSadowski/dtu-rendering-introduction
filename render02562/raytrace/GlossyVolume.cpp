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
  float3 rho_s = get_specular(hit);
  float s = get_shininess(hit);

    float3 L_r = make_float3(0.0f);
    float3 w_0 = (-1) * normalize(r.direction);

    for (int i = 0; i < lights.size(); i++) {
        float3 L_i, dir;
        if (lights[i]->sample(hit.position, dir, L_i)) {
            float3 w_i = dir;
            float3 n = normalize(hit.shading_normal);
            float3 w_r = 2 * dot(w_i, n) * n - w_i;

            float dr = fmax(dot(w_0, w_r), 0.0);
            float di = fmax(dot(w_i, n), 0.0);

            L_r += (rho_s * ((s + 2) * (M_1_PIf / 2)) * pow(dr, s)) * L_i * di;
        }

    }

    if(dot(hit.shading_normal, r.direction) > 0) {
        return L_r + Emission::shade(r, hit, emit) + Transparent::shade(r, hit, emit) * get_transmittance(hit);
    }

  return L_r + Emission::shade(r, hit, emit) + Volume::shade(r, hit, emit);
}
