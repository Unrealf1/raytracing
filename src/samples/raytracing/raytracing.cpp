#include "raytracing.h"
#include "float.h"

#include <iostream>
#include <cmath>

void RayTracer::load_cubemap(const std::array<std::string, 6>& paths) {
    for (size_t i = 0; i < paths.size(); ++i) {
        auto& path = paths[i];
        auto im_info = getImageInfo(path);
        std::cout << "loading cubemap with " << im_info.channels << " channels\n";
        std::cout << im_info.bytesPerChannel << " bytes per channel\n";
        m_cubemap[i] = {loadImageLDR(im_info), im_info};
    }
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

float3 sample_from_image(float2 uv, std::vector<unsigned char>& data, ImageFileInfo& info) {
    int x = int(uv[0] * info.width);
    int y = int(uv[1] * info.height);

    int iu = int(floor(uv[0] * info.width));
    int iv = int(floor(uv[1] * info.height));
    float u_r = uv[0] - float(iu);
    float v_r = uv[1] - float(iv);
    float u_inv = 1.0f - u_r;
    float v_inv = 1.0f - v_r;

    auto get_pixel_4 = [&](int index) -> float3 {
        int pixel_size = 4 * info.bytesPerChannel;
        unsigned char* current_data = data.data() + index * pixel_size;
        if (current_data - data.data() >= data.size() || current_data < data.data()) {
            current_data = data.data(); //TODO: do something more intelligent
        }

        auto color = create_color(*current_data, *(current_data + 1), *(current_data + 2));
        return destruct_color(color);
    };

    if (info.channels > 4 || info.channels < 3) {
        exit(1); //TODO: remove
    }
    auto width = info.width;
    return get_pixel_4(width * x + y);
    return (get_pixel_4(width * x + y) * u_inv + get_pixel_4(width * (x + 1) + y) * u_r) * v_inv +
           (get_pixel_4(width * x + y + 1) * u_inv + get_pixel_4(width * (x + 1) + y + 1) * u_r) * v_r;
}
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

void RayTracer::CastAARays(uint32_t tidX, uint32_t tidY, uint32_t* out_color, int num_aa_rays) {
  float3 final_color = {0.0f, 0.0f, 0.0f}; 
  auto color_index = tidY * m_width + tidX;
  for (int i = 0; i < num_aa_rays; ++i) {
      float offset_x = random_double();
      float offset_y = random_double();
      LiteMath::float4 rayPosAndNear, rayDirAndFar;
      kernel_InitEyeRay(tidX, tidY, &rayPosAndNear, &rayDirAndFar, offset_x, offset_y);
      kernel_RayTrace(tidX, tidY, &rayPosAndNear, &rayDirAndFar, out_color);
      uint32_t temp_color = out_color[color_index];
      final_color += destruct_color(temp_color);
  }
  final_color /= float(num_aa_rays);
  out_color[color_index] = create_color(final_color[0], final_color[1], final_color[2]);
}

void RayTracer::kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar, float offset_x, float offset_y)
{
  *rayPosAndNear = m_camPos; // to_float4(m_camPos, 1.0f);
  
  const LiteMath::float3 rayDir  = EyeRayDir(float(tidX) + offset_x, float(tidY) + offset_y, float(m_width), float(m_height), m_invProjView);
  *rayDirAndFar  = to_float4(rayDir, FLT_MAX);
}

const MaterialData_pbrMR& RayTracer::get_material_data(const CRT_Hit& hit) {

    if (m_scene_manager->m_materials.empty()) {
        static MaterialData_pbrMR fake_material = {};
        return fake_material;
    }
    auto mesh_info = m_scene_manager->GetMeshInfo(hit.geomId);
    uint32_t mat_id = m_scene_manager->m_matIDs[mesh_info.m_indexOffset / 3 + hit.primId];

    return m_scene_manager->m_materials[mat_id];
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
    if (k <= 0.0f) {
        result_color += base_color * light_color * ambient;
        return result_color;
    }
    float distance_k = dist_to_light < std::numeric_limits<float>::max() ? 1.0f / (dist_to_light * dist_to_light) : 1.0f;
    result_color += base_color * light_color * (k * distance_k + ambient);
    float blinn_k = powf(dot(reflection_dir, float3(0.0f, 0.0f, 0.0f) - ray_dir), blinn_pow);
    result_color += LiteMath::float3(1.0f, 1.0f, 1.0f) * specular * blinn_k * 0;
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
        int index = 1;
        float u;
        float v;
        convert_xyz_to_cube_uv(rayDir.x, rayDir.y, rayDir.z, &index, &u, &v);
        if (index == -1) {
            return background_color;
        }
        return sample_from_image({u, v}, m_cubemap[index].first, m_cubemap[index].second);
    } 

    auto normal = LiteMath::to_float4(get_normal_from_hit(hit), 0.0f);
    auto reflection_dir = LiteMath::normalize(LiteMath::reflect(to_float3(rayDir),to_float3(normal)));
    auto material = get_material_data(hit);
    auto result_color = LiteMath::float3{0.0f, 0.0f, 0.0f};
    auto base_color = destruct_color(m_palette[hit.instId % palette_size]);
    int glass_id = 2;
    int metal_id = 1;
    bool is_glass = hit.instId == glass_id;
    bool is_metal = hit.instId == metal_id;
    float refraction = is_glass ? 0.9f : 0.01f;
    material.metallic = 0.5f * random_double() * random_double(); //hit.instId % 3 == 0 ? 1.0f : 0.0f;
    if (is_metal) {
        material.metallic = 1.0f;
    }
    //auto base_color = LiteMath::float3(material.baseColor[0],material.baseColor[1],material.baseColor[2]);
    LiteMath::float3 hit_point = to_float3(rayPos) + normalize(to_float3(rayDir)) * hit.t;

    if (material.metallic > 0.0f && depth > 0) {
          result_color += material.metallic * trace(to_float4(hit_point, 0.0001f), to_float4(reflection_dir, FLT_MAX), background_color, depth - 1, diffuse_spread);
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
        if (light_hit.instId == uint32_t(-1) || light_hit.t >= dist_to_light || light_hit.instId == glass_id) {
            float k = light_hit.instId == glass_id ? refraction : 1.0f;
            auto light_color = light->getColor();
            result_color += k * calc_light_impact(
                dir_to_light, 
                dist_to_light, 
                reflection_dir, 
                to_float3(normal),
                to_float3(rayDir),
                base_color,
                light_color,
                material.metallic
            );
        } 
    }

    //TODO: not viable on my pc :(
    /*int random1 = depth + diffuse_spread + int(rayPos[1] * 100.0f) + int(rayDir[1] * 100.0f);
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
    */
    if (is_glass) {
        result_color *= (1-refraction);
        float3 refracted = refract(to_float3(rayDir), to_float3(normal), refraction); 
        result_color += refraction * trace(to_float4(hit_point, 0.0001f), to_float4(refracted, FLT_MAX), background_color, depth - 1, diffuse_spread);
    }
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

