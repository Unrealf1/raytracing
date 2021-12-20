#include "raytracing.h"
#include "float.h"
#include "fractals.hpp"

#include <cmath>
#include <limits>
#include <list>

using namespace fractals;


static constexpr float incorrect_val = std::numeric_limits<float>::infinity();
static bool is_correct_hit(float3 hit) {
    return hit[0] != incorrect_val;
}

static std::vector<SDF_base*>& get_sdfs() {
    static std::vector<SDF_base*> sdfs = {
        //make_fractal1({{1.0f, 0.5f, 0.2f}, 0.0f}),
        //make_sphere({0.0f, 20.0f, 0.0f}, 8.0f, {{1.0f, 0.5f, 0.1f}, 0.1f}),
        make_repeating(7.0f, {3.0f, 5.0f, 5.0f}, make_sphere({0.0f, 0.0f, 0.0f}, 5.0f, {{0.0f, 1.0f, 0.2f}, 0.1f})),
        //make_pyramid(0.1f, {{1.0f, 1.0f, 1.0f}, 0.2f}),
        //make_octahedron({0.0f, -40.0f, 20.0f}, 5.0f, {{1.0f, 1.0f, 1.0f}, 0.1f}),
        //make_octahedron_e({0.0f, -40.0f, -20.0f}, 5.0f, {{1.0f, 0.0f, 0.0f}, 0.1f})
    };

    return sdfs;
}

static std::pair<float, material> sdf_request(float3 position) {
    material mat;
    float dist = incorrect_val;

    auto& sdfs = get_sdfs();
    for (auto& sdf : sdfs) {
        float cur_dist = sdf->calc_distance(position);
        if (cur_dist < dist) {
            mat = sdf->get_material(position);
            dist = cur_dist;
        }
    }


    return {dist, mat};
}

static float distance_at(float3 position) {
    auto [dist, _] = sdf_request(position);
    return dist;
}


// from the doc
static float3 EstimateNormal(float3 z, float eps = 0.00001f)
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
    auto [current_distance, _] = sdf_request(ray_pos);
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
        int index = 1;
        float u;
        float v;
        convert_xyz_to_cube_uv(ray_dir.x, ray_dir.y, ray_dir.z, &index, &u, &v);
        if (index == -1) {
            return background_color;
        }
        return sample_from_image({u, v}, m_cubemap[index].first, m_cubemap[index].second);
    }

    auto& hit_pos = final_pos;
    auto normal = EstimateNormal(hit_pos);
    auto reflection_dir = LiteMath::normalize(LiteMath::reflect(ray_dir, normal));
    auto [distance, mat] = sdf_request(hit_pos);
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
        break; //TODO: remove
    }
    //std::cout << result_color[0] << std::endl;
    //exit(0);
    return result_color;
}


