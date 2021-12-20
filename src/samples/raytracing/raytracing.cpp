#include "raytracing.h"
#include "float.h"

#include <cmath>

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

    if (m_scene_manager->m_materials.empty()) {
        static MaterialData_pbrMR fake_material = {};
        return fake_material;
    }
    auto mesh_info = m_scene_manager->GetMeshInfo(hit.geomId);
    uint32_t mat_id = m_scene_manager->m_matIDs[mesh_info.m_indexOffset / 3 + hit.primId];

    return m_scene_manager->m_materials[mat_id];
}

//TODO: test
static uint32_t create_color(uint8_t red, uint8_t green, uint8_t blue) {
    return (red << 16) | (green << 8) | blue;
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

static float3 destruct_color(uint32_t color) {
    uint8_t r = (0x00ff0000 & color) >> 2 * 8;
    uint8_t g = (0x0000ff00 & color) >> 1 * 8;
    uint8_t b = 0x000000ff & color;
    return float3{r, g, b} / 255.0f;
}

float3 RayTracer::calc_light_impact(
        float3 dir_to_light,
        float dist_to_light,
        float3 reflection_dir,
        float3 normal,
        float3 ray_dir,
        float3 base_color,
        float3 light_color,
        float specular,
        float ambient,
        float blinn_pow

) {
    float3 result_color(0.0f, 0.0f, 0.0f);
    float k = std::max(dot(dir_to_light, normal), 0.0f);
    if (k <= 0.0f || true) {
        result_color += base_color * light_color * ambient * 5.0f;
        return result_color;
    }
    float distance_k = dist_to_light < std::numeric_limits<float>::max() ? 1.0f / (dist_to_light * dist_to_light) : 1.0f;
    result_color += base_color * light_color * (k * distance_k + ambient);
    float blinn_k = powf(dot(reflection_dir, float3(0.0f, 0.0f, 0.0f) - ray_dir), blinn_pow);
    result_color += LiteMath::float3(1.0f, 1.0f, 1.0f) * specular * blinn_k;
    return result_color;
}

// from "resources/shaders/unpack_attributes.h"
float3 DecodeNormal(uint32_t a_data)
{
  const uint32_t a_enc_x = (a_data  & 0x0000FFFFu);
  const uint32_t a_enc_y = ((a_data & 0xFFFF0000u) >> 16);
  const float sign   = (a_enc_x & 0x0001u) != 0 ? -1.0f : 1.0f;

  const int usX = int(a_enc_x & 0x0000FFFEu);
  const int usY = int(a_enc_y & 0x0000FFFFu);

  const int sX  = (usX <= 32767) ? usX : usX - 65536;
  const int sY  = (usY <= 32767) ? usY : usY - 65536;

  const float x = sX*(1.0f / 32767.0f);
  const float y = sY*(1.0f / 32767.0f);
  const float z = sign*sqrt(max(1.0f - x*x - y*y, 0.0f));

  return float3(x, y, z);
}


//TODO: test
float3 RayTracer::get_normal_from_hit(const CRT_Hit& hit) {
    using namespace LiteMath;
    const auto& mesh_info = m_scene_manager->GetMeshInfo(hit.geomId);

    auto mesh_data = m_scene_manager->GetMeshData();
    auto offset = 3 * hit.primId + mesh_info.m_indexOffset;

    float3 final_normal = {0.0f, 0.0f, 0.0f};

    for (size_t i = 0; i < 3; ++i) {
        uint32_t ii = mesh_data->IndexData()[offset + i];
        float* v = &(mesh_data->VertexData()[(mesh_info.m_vertexOffset + ii) * 8]);
        float3 normal = DecodeNormal(v[3]);
        final_normal += normal * hit.coords[3 - i];   
    }
    auto instance_matrix = m_scene_manager->GetInstanceMatrix(hit.instId);
    auto inversed = transpose(inverse4x4(instance_matrix));
    
    float3 normal = normalize(to_float3(inversed * to_float4(final_normal, 0.0f)));

    return normal;
}


float3 RayTracer::trace(float4 rayPos, float4 rayDir, float3 background_color, int depth, int diffuse_spread) {
    CRT_Hit hit = m_pAccelStruct->RayQuery_NearestHit(rayPos, rayDir);

    if (hit.instId == uint32_t(-1)) {
        return background_color;
    } 

    auto normal = LiteMath::to_float4(get_normal_from_hit(hit), 0.0f);
    auto reflection_dir = LiteMath::normalize(LiteMath::reflect(rayDir, normal));
    auto material = get_material_data(hit);
    //material.metallic = hit.instId % 3 == 0 ? 1.0f : 0.0f;
    auto result_color = LiteMath::float3{0.0f, 0.0f, 0.0f};
    //auto base_color = destruct_color(m_palette[hit.instId % palette_size]);
    auto base_color = LiteMath::float3(material.baseColor[0],material.baseColor[1],material.baseColor[2]);
    LiteMath::float3 hit_point = to_float3(rayPos) + normalize(to_float3(rayDir)) * hit.t;

    if (material.metallic > 0.0f && depth > 0) {
          result_color += material.metallic * trace(to_float4(hit_point, 0.0001f), reflection_dir, background_color, depth - 1, diffuse_spread);
    }

    if (material.metallic >= 1.0f) {
        return result_color;
    }

    for (auto& light : m_lights) {
        auto dir_to_light = light->getDirectionFrom(hit_point);
        auto dist_to_light = light->getDistanceFrom(hit_point);
        auto light_position = hit_point + dist_to_light * dir_to_light;
        auto light_hit = m_pAccelStruct->RayQuery_NearestHit(to_float4(hit_point, 0.0001f), LiteMath::to_float4(dir_to_light, FLT_MAX));
        // dist to directional is always at inf distance
        if (light_hit.instId == uint32_t(-1) || light_hit.t >= dist_to_light) {
            auto light_color = light->getColor();
            result_color += calc_light_impact(
                dir_to_light, 
                dist_to_light, 
                to_float3(reflection_dir), 
                to_float3(normal),
                to_float3(rayDir),
                base_color,
                light_color,
                material.metallic
            );
        } 
    }

    //TODO: test and fix
    int random1 = depth + diffuse_spread + int(rayPos[1] * 100.0f) + int(rayDir[1] * 100.0f);
    int random2 = random1 * random1;
    float3 additional_diffuse = {0.0f, 0.0f, 0.0f};
    
    float angle1 = 3.1415f * 2.0f / float(random1 % 100);
    float angle2 = 3.1415f * 2.0f / float(random2 % 100);
    for (int i = 0; i < diffuse_spread; ++i) {
        auto random_dir = normalize(LiteMath::float3(sin(angle1), cos(angle1), sin(angle2)));
        auto other_color = trace(to_float4(hit_point, 0.0001f), to_float4(random_dir, FLT_MAX), background_color, 0, 0);
        if ((other_color != background_color)[0] && (other_color != float3{0.0f, 0.0f, 0.0f})[0]) {
            additional_diffuse += other_color;
        }
    }
    additional_diffuse /= diffuse_spread;
    //result_color += additional_diffuse;

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

    auto color = m_is_marching ? 
        trace_marching(to_float3(rayPos), to_float3(rayDir), m_background_color, m_marching_steps, m_min_matching_distance, m_reflection_depth) : 
        trace(rayPos, rayDir, m_background_color, m_reflection_depth, m_diffuse_spread);
    out_color[tidY * m_width + tidX] = create_color(color[2], color[1], color[0]);
}

