// 02562 Rendering Framework
// Written by Jeppe Revall Frisvad, 2011
// Copyright (c) DTU Informatics 2011

#include <optix_world.h>
#include "HitInfo.h"
#include "Phong.h"

using namespace optix;

#ifndef M_1_PIf
#define M_1_PIf 0.31830988618379067154
#endif

float3 Phong::shade(const Ray& r, HitInfo& hit, bool emit) const
{
  float3 rho_d = get_diffuse(hit);
  float3 rho_s = get_specular(hit);
  float s = get_shininess(hit);

  // Implement Phong reflection here.
  //
  // Input:  r          (the ray that hit the material)
  //         hit        (info about the ray-surface intersection)
  //         emit       (passed on to Emission::shade)
  //
  // Return: radiance reflected to where the ray was coming from
  //
  // Relevant data fields that are available (see Lambertian.h, Ray.h, and above):
  // lights             (vector of pointers to the lights in the scene)
  // hit.position       (position where the ray hit the material)
  // hit.shading_normal (surface normal where the ray hit the material)
  // rho_d              (difuse reflectance of the material)
  // rho_s              (specular reflectance of the material)
  // s                  (shininess or Phong exponent of the material)
  //
  // Hint: Call the sample function associated with each light in the scene.
  float3 L_r = make_float3(0.0f);
  float3 w_0 = (-1) * normalize(r.direction);

  for (int i = 0; i < lights.size(); i++) {
      float3 L_i, dir;
      lights[i]->sample(hit.position, dir, L_i);

      float3 w_i = dir;
      float3 n = normalize(hit.shading_normal);
      float3 w_r = 2 * dot(w_i, n) * n - w_i;

      L_r += (rho_d * M_1_PIf + rho_s * (s + 2) * (M_1_PIf / 2) * pow(fmax(dot(w_0, w_r), 0.0), s) * L_i * fmax(dot(w_i, n), 0.0));
  }

//  return Lambertian::shade(r, hit, emit);
    return L_r;
}
