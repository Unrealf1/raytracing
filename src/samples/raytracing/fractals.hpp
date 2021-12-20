#pragma once

#include <LiteMath.h>


namespace fractals {
    struct material {
        float3 color = {0.0f, 0.0f, 0.0f};
        float metallic = 0.0f;
    };

    struct SDF_base {
        virtual float calc_distance(float3 position) = 0;
        virtual material get_material(float3 position) = 0;
        virtual ~SDF_base() = default;
    };

    template<typename Functor>
    struct SDF: public SDF_base {
        SDF(Functor f, material m): m_dist_impl(std::move(f)), m_material(std::move(m)) {}

        float calc_distance(float3 position) override {
            return m_dist_impl(position);
        }
        material get_material(float3 position) override {
            return m_material;
        }

        Functor m_dist_impl;
        material m_material;
    };

    
    float sdSphere(float3 position, float3 sphere_pos, float sphere_radius) {
        return LiteMath::length(sphere_pos - position) -  sphere_radius;
    }

    SDF_base* make_sphere(float3 sphere_pos, float sphere_radius, material mat) {
        auto this_sphere = [=](float3 position) { return sdSphere(position, sphere_pos, sphere_radius); };
        return new SDF<decltype(this_sphere)>( this_sphere, mat );
    }

    float sdPyramid( float3 p, float h) {
      float m2 = h*h + 0.25f;
       
      float2 pxz = {p.x, p.z};
      float2 pzx = {p.z, p.x};
      pxz = abs(pxz);
      pxz = (p.z>p.x) ? pzx : pxz;
      pxz -= 0.5f;

      float3 q = float3( p.z, h*p.y - 0.5*p.x, h*p.x + 0.5*p.y);
       
      float s = max(-q.x,0.0f);
      float t = clamp( (q.y-0.5f*p.z)/(m2+0.25f), 0.0f, 1.0f );
        
      float a = m2*(q.x+s)*(q.x+s) + q.y*q.y;
      float b = m2*(q.x+0.5*t)*(q.x+0.5*t) + (q.y-m2*t)*(q.y-m2*t);
        
      float d2 = min(q.y,-q.x*m2-q.y*0.5f) > 0.0f ? 0.0f : min(a,b);
        
      return sqrt( (d2+q.z*q.z)/m2 ) * sign(max(q.z,-p.y));
    }

    SDF_base* make_pyramid(float h, material mat) {
        auto this_sdf = [=](float3 position) { return sdPyramid(position, h); };
        return new SDF<decltype(this_sdf)>( this_sdf, mat );
    }

    float sdOctahedron( float3 p, float s)
    {
        p = abs(p);
        return (p.x+p.y+p.z-s)*0.57735027;
    }

    float sdOctahedron_exact( float3 p, float s)
    {
      p = abs(p);
      float m = p.x+p.y+p.z-s;
      float3 q;
           if( 3.0f*p.x < m ) q = p;
      else if( 3.0f*p.y < m ) q = {p[1], p[2], p[0]};
      else if( 3.0f*p.z < m ) q = {p[2], p[0], p[1]};
      else return m*0.57735027f;

      float k = clamp(0.5*(q.z-q.y+s),0.0,s);
      return length(vec3(q.x,q.y-s+k,q.z-k));
    }

    SDF_base* make_octahedron(float3 oct_pos, float s, material mat) {
        auto this_sdf = [=](float3 position) { return sdOctahedron(position + oct_pos, s); };
        return new SDF<decltype(this_sdf)>( this_sdf, mat );
    }
    SDF_base* make_octahedron_e(float3 oct_pos, float s, material mat, bool exact=false) {
        auto this_sdf_e = [=](float3 position) { return sdOctahedron_exact(position + oct_pos, s); };
        return new SDF<decltype(this_sdf_e)>( this_sdf_e, mat );
    }

    float opRepLim( float3 p, float c, float3 l, SDF_base* sdf )
    {
        float3 div = p / c;
        for (int i = 0; i < 3; ++i) div[i] = round(div[i]);
        float3 q = p-c*clamp(div,float3(0.0f, 0.0f, 0.0f)-l,l);
        return sdf->calc_distance( q );
    }

    SDF_base* make_repeating(float distance, float3 repetitions, SDF_base* sdf) {
        auto this_sdf = [=](float3 position) { return opRepLim(position, distance, repetitions, sdf); };
        return new SDF<decltype(this_sdf)>( this_sdf, sdf->get_material({0.0f, 0.0f, 0.0f}) );
    }
}
