#include "raytracing.h"
#include "float.h"

LiteMath::float3 EyeRayDir(float x, float y, float w, float h, LiteMath::float4x4 a_mViewProjInv)
{
  LiteMath::float4 pos = LiteMath::make_float4( 2.0f * (x + 0.5f) / w - 1.0f,
    2.0f * (y + 0.5f) / h - 1.0f,
    0.0f,
    1.0f );

  pos = a_mViewProjInv * pos;
  pos /= pos.w;

  //  pos.y *= (-1.0f);

  return normalize(to_float3(pos));
}

void RayTracer::CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color)
{
  LiteMath::float4 rayPosAndNear, rayDirAndFar;
  kernel_InitEyeRay(tidX, tidY, &rayPosAndNear, &rayDirAndFar);

  kernel_RayTrace(tidX, tidY, &rayPosAndNear, &rayDirAndFar, out_color);
}

void RayTracer::kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar)
{
  *rayPosAndNear = m_camPos; // to_float4(m_camPos, 1.0f);
  
  const LiteMath::float3 rayDir  = EyeRayDir(float(tidX), float(tidY), float(m_width), float(m_height), m_invProjView);
  *rayDirAndFar  = to_float4(rayDir, FLT_MAX);
}
/*
// calculate reflected ray direction
static LiteMath::float4 reflect(ray, hit) {
    return {};
}

// calculate refracted ray direction
static LiteMath::float4 refract(ray, hit) {
    return {};
}
*/

//TODO
const MaterialData_pbrMR& RayTracer::get_material_data(const CRT_Hit& hit) {
    // primid -> triangle
    // meshinfo ->indexoffset (оффсет троек)
    // indexoffset / 3 -> оффсет для всех примитивов данного меша
    // primitiveid -> 
    // meshbuffer[]
    return {};
}

static uint32_t create_color(uint8_t red, uint8_t green, uint8_t blue) {
    return red * 256 * 256 + green * 256 + blue * 256;
};

// arguments are from 0.0f to 1.0f
static uint32_t create_color(float red, float green, float blue) {
    auto int_red = static_cast<uint8_t>(std::lroundf(red * 255.0f));
    auto int_green = static_cast<uint8_t>(std::lroundf(green * 255.0f));
    auto int_blue = static_cast<uint8_t>(std::lroundf(blue * 255.0f));
    return create_color(int_red, int_green, int_blue);
};

static float3 mix_colors(float3 first, float3 second, float alpha) {
    alpha = std::max(0.0f, std::min(1.0f, alpha));
    return first * alpha + second * (1.0f - alpha);
}

//TODO
static float3 destruct_color(uint32_t color) {
    return {};
}

//TODO
static float3 calc_light_impact(float3 light_color) {
    return light_color;
}

//TODO
static float4 get_normal_from_hit(const CRT_Hit& hit) {
    return {};
}

float3 RayTracer::trace(float4 rayPos, float4 rayDir, float3 background_color) {
    CRT_Hit hit = m_pAccelStruct->RayQuery_NearestHit(rayPos, rayDir);

    if (hit.instId == uint32_t(-1)) {
        return background_color;
    }

    auto normal = get_normal_from_hit(hit);
    auto material = get_material_data(hit);
    auto result_color = destruct_color(m_palette[hit.instId % palette_size]);
    LiteMath::float4 hit_point = rayPos + rayDir * hit.t;
    auto hit_point_3 = LiteMath::to_float3(hit_point);
    for (auto& light : m_lights) {
        auto dir_to_light = light->getDirectionFrom(hit_point_3);
        auto dist_to_light = light->getDistanceFrom(hit_point_3);
        auto light_position = hit_point_3 + dist_to_light * dir_to_light;
        auto light_hit = m_pAccelStruct->RayQuery_NearestHit(hit_point, LiteMath::to_float4(dir_to_light, 0.0f));
        // dist to directional is always at inf distance
        if (light_hit.instId == uint32_t(-1) || light_hit.t >= dist_to_light) {
            auto light_color = light->getColorAt(hit_point_3);
            result_color += calc_light_impact(light_color);
        }
    }

    if (material.metallic > 0.0f) {
          auto new_dir = LiteMath::reflect(rayDir, normal);
          result_color += material.metallic * trace(hit_point, new_dir);
    }
    /*

    if (hit.material.refraction > 0)
    {
      Ray refrRay = refract(ray, hit);
      color += hit.material.refraction*RayTrace(refrRay);
    }*/

    return result_color;
}


void RayTracer::kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint32_t* out_color)
{
    const LiteMath::float4 rayPos = *rayPosAndNear;
    const LiteMath::float4 rayDir = *rayDirAndFar ;

    auto color = trace(rayPos, rayDir);

    out_color[tidY * m_width + tidX] = create_color(color[0], color[1], color[2]);
}
