#ifndef VK_GRAPHICS_RT_RAYTRACING_H
#define VK_GRAPHICS_RT_RAYTRACING_H

#include <cstdint>
#include <random>
#include <memory>
#include <iostream>
#include "LiteMath.h"
#include "render/CrossRT.h"
#include "../../render/scene_mgr.h"
#include "Light.h"
#include "loader_utils/image_loader.h"



inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

class RayTracer
{
public:
  RayTracer(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height) {}

  void UpdateView(const LiteMath::float3& a_camPos, const LiteMath::float4x4& a_invProjView ) { m_camPos = to_float4(a_camPos, 1.0f); m_invProjView = a_invProjView; }
  void SetScene(std::shared_ptr<ISceneObject> a_pAccelStruct) { m_pAccelStruct = a_pAccelStruct; };
  void SetSceneManager(std::shared_ptr<SceneManager> scene_manager) { m_scene_manager = std::move(scene_manager); };

  void CastSingleRay(uint32_t tidX, uint32_t tidY, uint32_t* out_color);
  void CastAARays(uint32_t tidX, uint32_t tidY, uint32_t* out_color, int num_aa_rays);
  void kernel_InitEyeRay(uint32_t tidX, uint32_t tidY, LiteMath::float4* rayPosAndNear, LiteMath::float4* rayDirAndFar, float offset_x = 0.0f, float offset_y = 0.0f);
  void kernel_RayTrace(uint32_t tidX, uint32_t tidY, const LiteMath::float4* rayPosAndNear, const LiteMath::float4* rayDirAndFar, uint32_t* out_color);
  void load_cubemap(const std::array<std::string, 6>& paths);
  void AddLight(LightInfo* light) { m_lights.push_back(light); }

  float3 m_background_color = {0.15f, 0.15f, 0.15f};
  float m_min_matching_distance = 1.0e-3f;
  int m_marching_steps = 30;
  int m_reflection_depth = 1;
  int m_diffuse_spread = 3;
  int m_aa_rays = 4;
  bool m_is_marching = false;
protected:
  uint32_t m_width;
  uint32_t m_height;

  LiteMath::float4   m_camPos;
  LiteMath::float4x4 m_invProjView;

  std::shared_ptr<ISceneObject> m_pAccelStruct;
  std::vector<LightInfo*> m_lights;
  std::shared_ptr<SceneManager> m_scene_manager;

  std::array<std::pair<std::vector<unsigned char>, ImageFileInfo>, 6> m_cubemap = {};
  int m_cubemap_width = 0;
  int m_cubemap_height = 0;

  static constexpr uint32_t palette_size = 20;
  // color palette to select color for objects based on mesh/instance id
  static constexpr uint32_t m_palette[palette_size] = {
    0xffe6194b, 0xff3cb44b, 0xffffe119, 0xff0082c8,
    0xfff58231, 0xff911eb4, 0xff46f0f0, 0xfff032e6,
    0xffd2f53c, 0xfffabebe, 0xff008080, 0xffe6beff,
    0xffaa6e28, 0xfffffac8, 0xff800000, 0xffaaffc3,
    0xff808000, 0xffffd8b1, 0xff000080, 0xff808080
  };



  const MaterialData_pbrMR& get_material_data(const CRT_Hit& hit);
  // returns color
  float3 trace(float4 rayPos, float4 rayDir, float3 background_color, int depth, int diffuse_spread);
  float3 trace_marching(float3 rayPos, float3 rayDir, float3 background_color, int steps, float min_dist, int depth);
  static float3 trace_marching_pos(float3 ray_pos, float3 ray_dir, int steps, float min_dist);

  float3 get_normal_from_hit(const CRT_Hit& hit);
  static float3 calc_light_impact(
        float3 dir_to_light,
        float dist_to_light,
        float3 reflection_dir,
        float3 normal,
        float3 ray_dir,
        float3 base_color,
        float3 light_color,
        float ambient=0.1f,
        float specular = 0.5f,
        float blinn_pow = 3.0f);
};
  inline void convert_xyz_to_cube_uv(float x, float y, float z, int *index, float *u, float *v)
  {
    float absX = fabs(x);
    float absY = fabs(y);
    float absZ = fabs(z);
      
    int isXPositive = x > 0 ? 1 : 0;
    int isYPositive = y > 0 ? 1 : 0;
    int isZPositive = z > 0 ? 1 : 0;
      
    float maxAxis, uc, vc;
      
    // POSITIVE X
    if (isXPositive && absX >= absY && absX >= absZ) {
      // u (0 to 1) goes from +z to -z
      // v (0 to 1) goes from -y to +y
      maxAxis = absX;
      uc = -z;
      vc = y;
      *index = 0;
    }
    // NEGATIVE X
    if (!isXPositive && absX >= absY && absX >= absZ) {
      // u (0 to 1) goes from -z to +z
      // v (0 to 1) goes from -y to +y
      maxAxis = absX;
      uc = z;
      vc = y;
      *index = 1;
    }
    // POSITIVE Y
    if (isYPositive && absY >= absX && absY >= absZ) {
      // u (0 to 1) goes from -x to +x
      // v (0 to 1) goes from +z to -z
      maxAxis = absY;
      uc = x;
      vc = -z;
      *index = 2;
    }
    // NEGATIVE Y
    if (!isYPositive && absY >= absX && absY >= absZ) {
    // u (0 to 1) goes from -x to +x
      // v (0 to 1) goes from -z to +z
      maxAxis = absY;
      uc = x;
      vc = z;
      *index = 3;
    }
    // POSITIVE Z
    if (isZPositive && absZ >= absX && absZ >= absY) {
      // u (0 to 1) goes from -x to +x
      // v (0 to 1) goes from -y to +y
      maxAxis = absZ;
      uc = x;
      vc = y;
      *index = 4;
    }
    // NEGATIVE Z
    if (!isZPositive && absZ >= absX && absZ >= absY) {
      // u (0 to 1) goes from +x to -x
      // v (0 to 1) goes from -y to +y
      maxAxis = absZ;
      uc = -x;
      vc = y;
      *index = 5;
    }

    // Convert range from -1 to 1 to 0 to 1
    *u = 0.5f * (uc / maxAxis + 1.0f);
    *v = 0.5f * (vc / maxAxis + 1.0f);
  }

  float3 sample_from_image(float2 uv, std::vector<unsigned char>& data, ImageFileInfo& info);
     

#endif// VK_GRAPHICS_RT_RAYTRACING_H
