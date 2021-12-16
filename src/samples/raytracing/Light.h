#pragma once

#include "LiteMath.h"

#include <limits>


struct LightInfo {
    // direction TO the light, from the point
    virtual LiteMath::float3 getDirectionFrom(const LiteMath::float3& point) = 0;

    // color at given point
    virtual LiteMath::float3 getColorAt(const LiteMath::float3& point) = 0;

    virtual float getDistanceFrom(const LiteMath::float3& point) = 0;
    virtual ~LightInfo() = default;
};

struct DirectionalLight: public LightInfo {
public:
    DirectionalLight(LiteMath::float3 color, LiteMath::float3 direction)
        : m_color(color)
        , m_direction(direction) {}

    LiteMath::float3 getDirectionFrom(const LiteMath::float3&)override { return { -m_color[0], -m_color[1], -m_color[2]  }; } 
    LiteMath::float3 getColorAt(const LiteMath::float3&) override{ return m_color; };
    float getDistanceFrom(const LiteMath::float3&)override { return std::numeric_limits<float>::infinity(); };

    LiteMath::float3 m_color;
    LiteMath::float3 m_direction;
};

struct PointLight: public LightInfo {
public:
    PointLight(LiteMath::float3 color, LiteMath::float3 position)
        : m_color(color)
        , m_position(position) {}

    LiteMath::float3 getDirectionFrom(const LiteMath::float3& from) override{ return { m_position[0] - from[0], m_position[1] - from[1], m_position[2] - from[2] }; } 
    LiteMath::float3 getColorAt(const LiteMath::float3& point)override {
        // turns out they DO have these operations...
        auto sum_sq = m_color[0]*m_color[0] + m_color[1]*m_color[1] + m_color[2]*m_color[2];
        auto distance = std::sqrt(sum_sq);   
        return m_color / distance;
    }; 
    float getDistanceFrom(const LiteMath::float3& point) override {
        return LiteMath::length(point - m_position);
    }

    LiteMath::float3 m_color;
    LiteMath::float3 m_position;
};
