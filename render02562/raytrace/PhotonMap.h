//----------------------------------------------------------------------
//  
// This file was scanned from Henrik Wann Jensen: Realistic image 
// Sythesis Using Photon Mapping, A K Peters, July 2001, by Marianne 
// Bilsted Pedersen, Anders Kyster & Olav Madsen
//
// It has been slightly modified (October 2002) by Andreas B�rentzen
// in order to use CGLA my Linear Algebra library.
//
// Slightly modified (March 2009) by Jeppe Revall Frisvad to enable
// rendering of the photons as points.
//
// Templatified (November 2010) by Jeppe Revall Frisvad to enable 
// mapping of different types of photons. All code is now in this 
// header file.
//
// Switched from CGLA to OptiX vectors and Aabb (July 2011), 
// Jeppe Revall Frisvad.
//
// Bugs fixed that gave three black pixels in splatting (June 2012),
// Jeppe Revall Frisvad.
//
//----------------------------------------------------------------------

#ifndef __PHOTONMAP_H__
#define __PHOTONMAP_H__
 
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <optix_world.h>
#include "my_glut.h"

#ifdef _OPENMP
  #include <omp.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//This is the photon
//The power is not compressed so the size is 28 bytes
struct Photon
{
  optix::float3 pos;            //photon position
  short plane;                  //splitting plane for kd_tree
  unsigned char theta, phi;     //incoming direction
  optix::float3 power;          //photon power (uncompressed)
  unsigned int m_type;
};

// This structure is used only to locate the nearest photons
template<class T>
struct NearestPhotons
{
  int max;
  int found;
  int got_heap;
  optix::float3 pos;
  float *dist2;
  const T** index;
};

//The following swap function was a define macro in the cpp-file.
template<class T>
inline void photon_swap(T** ph, int a, int b) { T* ph2=ph[a]; ph[a]=ph[b]; ph[b]=ph2; }

//This is the PhotonMap class
template<class T = Photon> 
class PhotonMap
{
public:
  
  /* This is the constructor for the photon map.
     To create the photon map it is necessary to specify the
     maximum number of photons that will be stored  */
  PhotonMap(int max_phot)
  {
    stored_photons = 0;
    prev_scale = 1;
    max_photons = max_phot;

    // Allocates an array for the photons (OM)
    photons = (T*)malloc(sizeof(T)*(max_photons + 1));

    if(photons == 0)
    {
      fprintf(stderr,"Out of memory initializing photon map\n");
      exit(-1);
    }

    // initialize direction conversion tables
    for(int i = 0; i < 256; ++i)
    {
      double angle = double(i)*(1.0/256.0)*M_PI;
      costheta[i]  = static_cast<float>(std::cos(angle));
      sintheta[i]  = static_cast<float>(std::sin(angle));
      cosphi[i]    = static_cast<float>(std::cos(2.0*angle));
      sinphi[i]    = static_cast<float>(std::sin(2.0*angle));
    }
  }

  ~PhotonMap() { std::free(photons); }

  int get_photon_count() const { return stored_photons; }
  int get_max_photon_count() const { return max_photons; }

  /* store puts a Photon into the flat array that will form
     the final kd-tree.
     Call this function to store a photon. */
  void store(
    const optix::float3& power,  // photon power
    const optix::float3& pos,    // photon position
    const optix::float3& dir)    // photon direction
  {
    if(stored_photons >= max_photons)
      return;
   
    if(power.x + power.y + power.z < 1.0e-8f)
      return;

    T* node = 0;
    #pragma omp critical (store_photon)
    {
      ++stored_photons;
      node = &photons[stored_photons];
    }

    node->pos = pos;
    bbox.include(pos);
    node->power = power;

    int theta = int(std::acos(dir.z)*(256.0/M_PI));
    if(theta > 255)
      node->theta = 255;
    else
      node->theta = (unsigned char)theta;

    int phi = int(std::atan2(dir.y, dir.x)*(256.0/(2.0*M_PI)));
    if(phi > 255)
      node->phi = 255;
    else if(phi < 0)
      node->phi = (unsigned char)(phi + 256);
    else
      node->phi = (unsigned char)phi;
  }
    
  /* scale-photon-power is used to scale the power of all
     photons once they have been emitted from the light
     source. scale = 1/(*emitted photons).
     Call this function after each light source is processed. */
  void scale_photon_power(
    const float scale)        // l/(number of emitted photons)
  {
    #pragma omp parallel for
    for(int i = prev_scale; i <= stored_photons; ++i)
      photons[i].power *= scale;

    mean_scale = (stored_photons - prev_scale - 1)*scale + (prev_scale - 1)*mean_scale;
    mean_scale /= static_cast<float>(stored_photons);
    prev_scale = stored_photons + 1;
  }

  // balance creates a left-balanced kd-tree from the flat photon array.
  // This function should be called before the photon map
  // is used for rendering.
  void balance(void)           // balance the kd_tree (before use!)
  {
    if(stored_photons > 1)
    {
      // allocate two temporary arrays for the balancing procedure
      T** pa1 = (T**)std::malloc(sizeof(T*)*(stored_photons + 1));
      T** pa2 = (T**)std::malloc(sizeof(T*)*(stored_photons + 1));

      int i;
      for(i = 0; i <= stored_photons; ++i)
        pa2[i] = &photons[i];  

      balance_segment(pa1, pa2, 1, 1, stored_photons);
      std::free(pa2);

      // reorganize balanced kd_tree (make a heap)
      int d, j = 1, foo = 1;
      T foo_photon = photons[j];
      for(i = 1; i <= stored_photons; ++i)
      {
        d = pa1[j] - photons;
        pa1[j] = 0;
        if(d != foo)
          photons[j] = photons[d];
        else
        {
          photons[j] = foo_photon;
          if(i < stored_photons)
          {
            for(; foo <= stored_photons; ++foo)
              if(pa1[foo] != 0)
                break;
            foo_photon = photons[foo];
            j = foo;
          }
          continue;
        }
        j = d;
      }
      free(pa1);
    }
    half_stored_photons = stored_photons/2;
  }
  
  //irradiance_estimate computes an irradiance estimate at a given surface position
  const optix::float3 irradiance_estimate(
    const optix::float3& pos,             // surface position
    const optix::float3& normal,          // surface normal at pos
    const float max_dist,                 // max distance to look for photons
    const int nphotons) const             // number of photons to use
  {
    NearestPhotons<T> np;
    np.dist2 = (float*)alloca(sizeof(float)*(nphotons + 1));
    np.index = (const T**)alloca(sizeof(T*)*(nphotons + 1));

    np.pos = pos;
    np.max = nphotons;
    np.found = 0;
    np.got_heap = 0;
    np.dist2[0] = max_dist*max_dist;

    // locate_photons finds the nearest photons in the map given the parameters in np
    locate_photons(&np, 1);

    // sum irradiance from all photons
    optix::float3 irrad = optix::make_float3(0.0f);
    for(int i = np.found; i > 0; --i)
    {
      const T* p = np.index[i];
      // the photon_dir call and following if can be omitted (for speed)
      // if the scene does not have any thin surfaces
      if(dot(photon_dir(p), normal) > 0.0f)
      {        
        irrad += p->power;
      }
    }
    irrad *= 1.0f/(M_PIf*np.dist2[0]);  // estimate of density
    return irrad;
  }

  void locate_photons(
    NearestPhotons<T>* const np,        // np is used to locate the photons
    const int index) const              // call with index = 1
  {
    const T* p = &photons[index];
    float dist1;

    if(index <= half_stored_photons)
    {
      dist1 = *(&np->pos.x + p->plane) - *(&p->pos.x + p->plane);
      if(dist1 > 0.0)     // if dist1 is positive search right plane 
      {
        locate_photons(np, 2*index + 1);
        if(dist1*dist1 < np->dist2[0])
          locate_photons(np, 2*index);
      }                   // end if
      else                // dist1 is negative, search left first
      {
        locate_photons(np, 2*index);
        if(dist1*dist1 < np->dist2[0])
          locate_photons(np, 2*index + 1);
      }                   // end else
    } // end if  
    
    // compute squared distance between current photon and np->pos
    optix::float3 vdist = p->pos - np->pos;
    float dist2 = dot(vdist, vdist);

    if(dist2 < np->dist2[0])
    {
      // we found a photon :) Insert it in the candidate list
      if(np->found < np->max)
      {
        // heap is not full; use array
        ++np->found;
        np->dist2[np->found] = dist2;
        np->index[np->found] = p;
      } // end if
      else
      {
        int j, parent;
        if(np->got_heap == 0)  // Do we need to build the heap?
        {
          // Build heap
          float dst2;
          const T* phot;
          int half_found = np->found>>1;
          for(int k = half_found; k >= 1; --k)
          {
            parent = k;
            phot = np->index[k];
            dst2 = np->dist2[k];
            while(parent <= half_found)
            { 
              j = parent + parent;
              if(j < np->found && np->dist2[j] < np->dist2[j + 1])
                ++j;
              if(dst2 >= np->dist2[j])
                break;
              np->dist2[parent] = np->dist2[j];
              np->index[parent] = np->index[j];
              parent = j;
            } // end while
            np->dist2[parent] = dst2; 
            np->index[parent] = phot;
          } // end for
          np->got_heap = 1;
        } // end if 
      
        // insert new photon into max heap
        // delete largest element, insert new, and reorder the heap
        parent = 1;
        j = 2;
        while(j <= np->found)
        { 
          if(j < np->found && np->dist2[j] < np->dist2[j + 1])
            j++;
          if(dist2 > np->dist2[j])
            break;
          np->dist2[parent] = np->dist2[j];
          np->index[parent] = np->index[j];
          parent = j;
          j += j;
        } // end while
        if(dist2 < np->dist2[parent])
        {
          np->index[parent] = p;
          np->dist2[parent] = dist2;
        }
        np->dist2[0] = np->dist2[1];
      } // end else
    } // end if
  } // end locate_photons

  // returns the direction of a photon
  const optix::float3 photon_dir(
    const T* p) const               // the photon
  {
    optix::float3 dir;
    dir.x = sintheta[p->theta]*cosphi[p->phi];
    dir.y = sintheta[p->theta]*sinphi[p->phi];
    dir.z = costheta[p->theta];
    return dir;
  }

  void draw()
  {
    if(!glIsList(disp_list))
    {
      disp_list = glGenLists(1);
      glNewList(disp_list, GL_COMPILE);

      glBegin(GL_POINTS);
      for(int i = 1; i <= stored_photons; ++i)
      {
        optix::float3 color = photons[i].power/mean_scale; //*1.0e-5f;
        glColor3fv(&color.x);
        glVertex3fv(&photons[i].pos.x);
      }
      glEnd();

      glEndList();
    }
    glCallList(disp_list);
  }

private:

  // See "Realistic Image Synthesis using Photon Mapping" chapter 6 
  // for an explanation of this function
  void balance_segment(
    T** pbal,
    T** porg,
    const int index,
    const int start,
    const int end)
  {
    // compute new median
    int median = 1;
    while(4*median <= end - start + 1)
      median += median;

    if(3*median <= end - start + 1)
    {
      median += median;
      median += start - 1;
    }
    else
      median = end - median + 1;

    // find axis to split along 
    int axis = 2;
    optix::float3 extent = bbox.extent();
    if(extent.x > extent.y && extent.x > extent.z)
      axis = 0;
    else if(extent.y > extent.z)
      axis = 1;

    // partition photon block around the median
    median_split(porg, start, end, median, axis);

    pbal[index] = porg[median];
    pbal[index]->plane = axis;

    // recursively balance the left and right block
    if(median > start)
    {
      // balance left segment
      if(start < median - 1)
      {
        float& bbox_max_axis = *(&bbox.m_max.x + axis);
        const float tmp = bbox_max_axis;
        bbox_max_axis = *(&pbal[index]->pos.x + axis);
        balance_segment(pbal, porg, 2*index, start, median - 1);
        bbox_max_axis = tmp;
      }
      else
        pbal[2*index] = porg[start];
    }

    if(median < end)
    {
      // balance right segment
      if(median + 1 < end)
      {
        float& bbox_min_axis = *(&bbox.m_min.x + axis);
        const float tmp = bbox_min_axis;
        bbox_min_axis = *(&pbal[index]->pos.x + axis);
        balance_segment(pbal, porg, 2*index + 1, median + 1, end);
        bbox_min_axis = tmp;
      }
      else
        pbal[2*index + 1] = porg[end];
    }
  }

  // median_split splits the photon array into two separate
  // pieces around the median, with all photons below the
  // median in the lower half and all photons above
  // the median in the upper half. The comparison
  // criteria is the axis ( indicated by the axis parameter )
  // ( inspired by routine in Sedgewick "Algoritms in C++ )
  void median_split(
    T** p,
    const int start,
    const int end,
    const int median,
    const int axis)
  {
    int left = start;
    int right = end;

    while(right > left)
    {
      const float v = *(&p[right]->pos.x + axis);
      int i = left - 1;
      int j = right;
      for(;;)
      {
        while(*(&p[++i]->pos.x + axis) < v);
        while(*(&p[--j]->pos.x + axis) > v && j > left);
        if(i >= j)
          break;
        photon_swap(p, i, j);
      }

      photon_swap(p, i, right);
      if(i >= median)
        right = i - 1;
      if(i <= median)
        left = i + 1;
    }
  }

protected:
  T* photons;

  int stored_photons;
  int half_stored_photons;
  int max_photons;
  int prev_scale;

  float costheta[256];
  float sintheta[256];
  float cosphi[256];
  float sinphi[256];

  optix::Aabb bbox;

  unsigned int disp_list;
  float mean_scale;
};

#endif // __PHOTONMAP_H__
