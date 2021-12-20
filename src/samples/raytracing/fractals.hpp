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

    
    float sdPlane(float3 position, float height) {
        return abs(position.y);
    }

    SDF_base* make_plane(float height, material mat) {
        auto this_sdf = [=](float3 position) {
            return sdPlane(position, height);
        };
        return new SDF<decltype(this_sdf)>( this_sdf, mat );
    }

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
      return length(float3(q.x,q.y-s+k,q.z-k));
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

    float fractal1( float3 p ) {
        float x3 = p.x*p.x*p.x;
        float x2 = p.x*p.x;
        float x = p.x;

        float y3 = p.y*p.y*p.y;
        float y2 = p.y*p.y;
        float y = p.y;
        
        float z3 = p.z*p.z*p.z;
        float z2 = p.z*p.z;
        float z = p.z;

        float v1 = (x3 - 3*x*y2 - 3*x*z2);
        float v2 = (y3 - 3*y*x2 - 3*y*z2);
        float v3 = (z3 - 3*z*x2 - 3*z*y2);
        float v4 = (x2 + y2 + z2);

        return v1*v1 + v2*v2 + v3*v3 - v4*v4*v4;
    }

    void sphereFold(float3& z, float& dz) {
        float r = 1.0f;
        float minRadius2 = 1.0f;
        float fixedRadius2 = 1.0f;
        

        float r2 = dot(z,z);
        if (r<minRadius2) { 
            // linear inner scaling
            float temp = (fixedRadius2/minRadius2);
            z *= temp;
            dz*= temp;
        } else if (r2<fixedRadius2) { 
            // this is the actual sphere inversion
            float temp =(fixedRadius2/r2);
            z *= temp;
            dz*= temp;
        }
    }

    void boxFold(float3& z, float& dz) {
        float foldingLimit = 10.0f;
        z = clamp(z, -foldingLimit, foldingLimit) * 2.0 - z;
    }

    float fractal2 (float3 z) {
        float3 offset = z;
        float dr = 1.0f;
        int Iterations = 30;
        float Scale = 2.0f;
        for (int n = 0; n < Iterations; n++) {
            boxFold(z,dr);       // Reflect
            sphereFold(z,dr);    // Sphere Inversion
            
                    z=Scale*z + offset;  // Scale & Translate
                    dr = dr*abs(Scale)+1.0f;
        }
        float r = length(z);
        return r/abs(dr);
    
    }

    float fractal3 (float3 pos) {
        float scale = 1.0f;
        float DEfactor = scale;

        float fixedRadius = 1.0f;
        float fR2 = fixedRadius * fixedRadius;
        float minRadius = 0.5f;
        float mR2 = minRadius * minRadius;

        float x = pos.x;
        float y = pos.y;
        float z = pos.z;

        if (x > 1.0f) x = 2.0f - x;
        else if (x < -1.0f) x = -2.0f - x;
        if (y > 1.0f)
        y = 2.0f - y;
        else if (y < -1.0f) y = -2.0f - y;
        if (z > 1.0f)
        z = 2.0f - z;
        else if (z < -1.0f) z = -2.0f - z;

        float r2 = x*x + y*y + z*z;

        if (r2 < mR2)
        {
           x = x * fR2 / mR2;
           y = y * fR2 / mR2;
           z = z * fR2 / mR2;
           DEfactor = DEfactor * fR2 / mR2;
        }
        else if (r2 < fR2)
        {
           x = x * fR2 / r2;
           y = y * fR2 / r2;
           z = z * fR2 / r2;
           DEfactor *= fR2 / r2;
        }
        
        float cx = 0.0f;
        float cy = 0.0f;
        float cz = 0.0f;

        x = x * scale + cx;
        y = y * scale + cy;
        z = z * scale + cz;
        DEfactor *= scale;

        return sqrt(x*x+y*y+z*z)/abs(DEfactor);
    }

    float fractal4(float3 p, int iterations = 3) {
        return sdPyramid( p, 1.0f);
    }

    float fractal5 (float3 z, int Iterations = 30, float Scale = 2.0f) {
        // create a simple tetrahedron
        float3 a1 = float3(1.0f, 1.0f, 1.0f);
        float3 a2 = float3(-1.0f, -1.0f, 1.0f);
        float3 a3 = float3(1.0f,-1.0f,-1.0f);
        float3 a4 = float3(-1.0f,1.0f,-1.0f);
        float3 c;
        int n = 0;
        float dist, d;
        while (n < Iterations) {
             // choose point, closest to the position
             c = a1; dist = length(z-a1);
             d = length(z-a2); if (d < dist) { c = a2; dist=d; }
             d = length(z-a3); if (d < dist) { c = a3; dist=d; }
             d = length(z-a4); if (d < dist) { c = a4; dist=d; }
             z = Scale*z-c*(Scale-1.0f);
             n++;
        }

        return length(z) * pow(Scale, float(-n));

    }

    float infinite_spheres(float3 z)
    {
      auto copy = z;
      copy.x = round(z.x);
      copy.y = round(z.y);
      copy.z = 0.0f;
      return sdSphere(z, copy, 0.3f);
    }

    SDF_base* make_fractal1(material mat) {
        auto this_sdf = [](float3 position) { return infinite_spheres(position); };
        return new SDF<decltype(this_sdf)>( this_sdf, mat );
    }
    SDF_base* make_fractal2(material mat) {
        auto this_sdf = [](float3 position) { return fractal5(position); };
        return new SDF<decltype(this_sdf)>( this_sdf, mat );
    }
    SDF_base* make_fractal3(material mat) {
        auto this_sdf = [](float3 position) { return fractal2(position); };
        return new SDF<decltype(this_sdf)>( this_sdf, mat );
    }

}
