#include "raytracing.h"
#include "float.h"

#include <cmath>
#include <limits>


struct material {
    float3 color = {0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
};

static constexpr float incorrect_val = std::numeric_limits<float>::infinity();
static bool is_correct_hit(float3 hit) {
    return hit[0] != incorrect_val;
}

static float sphere(float3 position, float3 sphere_pos, float sphere_radius) {
    return LiteMath::length(sphere_pos - position) -  sphere_radius;
}

static float distance_at(float3 position) {
    return std::min(
        sphere(position, {0.0f, 0.0f, 0.0f}, 5.0f),
        sphere(position, {0.0f, 20.0f, 0.0f}, 8.0f)
    );
}

static material material_at(float3 position) {
    auto dist1 = sphere(position, {0.0f, 0.0f, 0.0f}, 5.0f);
    auto dist2 = sphere(position, {0.0f, 2.0f, 0.0f}, 8.0f);
    float m = dist1 < dist1 ? 1.0f : 0.1f;

    return {{1.0f, 0.5f, 0.1f}, m};
}

// from the doc
static float3 EstimateNormal(float3 z, float eps = 0.001f)
{
        float3 z1 = z + float3(eps, 0, 0);
        float3 z2 = z - float3(eps, 0, 0);
        float3 z3 = z + float3(0, eps, 0);
        float3 z4 = z - float3(0, eps, 0);
        float3 z5 = z + float3(0, 0, eps);
        float3 z6 = z - float3(0, 0, eps);
        float dx = distance_at(z1) - distance_at(z2);
        float dy = distance_at(z3) - distance_at(z4);
        float dz = distance_at(z5) - distance_at(z6);
        return normalize(float3(dx, dy, dz) / (2.0*eps));
}

float3 RayTracer::trace_marching_pos(float3 ray_pos, float3 ray_dir, int steps, float min_dist) {
    if (steps == 0) {
        return {incorrect_val, incorrect_val, incorrect_val};
    }

    float current_distance = distance_at(ray_pos);
    auto new_pos = ray_pos + ray_dir * current_distance;
    float new_distance = distance_at(new_pos);
    if (new_distance <= min_dist) {
        return new_pos;
    }
    return trace_marching_pos(new_pos, ray_dir, steps - 1, min_dist);
}

float3 RayTracer::trace_marching(float3 ray_pos, float3 ray_dir, float3 background_color, int steps, float min_dist, int depth) {
    auto final_pos = trace_marching_pos(ray_pos, ray_dir, steps, min_dist);
    if (!is_correct_hit(final_pos)) {
        return background_color;
    }

    auto& hit_pos = final_pos;
    auto normal = EstimateNormal(hit_pos);
    auto reflection_dir = LiteMath::normalize(LiteMath::reflect(ray_dir, normal));
    auto mat = material_at(hit_pos);
    auto base_color = mat.color;
    float3 result_color(0.0f, 0.0f, 0.0f);

    if (mat.metallic > 0.0f && depth > 0) {
        result_color += mat.metallic * trace_marching(hit_pos, reflection_dir, background_color, steps, min_dist, depth - 1);
    }

    if (mat.metallic >= 1.0f) {
        return result_color;
    }

    for (auto& light : m_lights) {
        auto dir_to_light = light->getDirectionFrom(hit_pos);
        auto dist_to_light = light->getDistanceFrom(hit_pos);
        auto light_position = hit_pos + dist_to_light * dir_to_light;
        auto light_hit_pos = trace_marching_pos(hit_pos, dir_to_light, steps, min_dist);
        auto light_hit_distance = LiteMath::length(light_hit_pos - hit_pos);
        // dist to directional is always at inf distance
        if (!is_correct_hit(light_hit_pos) || light_hit_distance >= dist_to_light) {
            auto light_color = light->getColor();
            result_color += calc_light_impact(
                dir_to_light, 
                dist_to_light,
                reflection_dir,
                normal,
                ray_dir,
                base_color,
                light_color,
                mat.metallic
            );
        } 
    }
    //std::cout << result_color[0] << std::endl;
    //exit(0);
    return result_color;
}


