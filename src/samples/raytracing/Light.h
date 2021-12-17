#pragma once

#include "LiteMath.h"

#include <limits>


struct LightInfo {
    // direction TO the light, from the point
    virtual LiteMath::float3 getDirectionFrom(const LiteMath::float3& point) = 0;

    // color at given point
    virtual LiteMath::float3 getColor() = 0;

    virtual float getDistanceFrom(const LiteMath::float3& point) = 0;
    virtual ~LightInfo() = default;
};

struct DirectionalLight: public LightInfo {
public:
    DirectionalLight(LiteMath::float3 color, LiteMath::float3 direction)
        : m_color(color)
        , m_direction(direction) {}

    LiteMath::float3 getDirectionFrom(const LiteMath::float3&) override { return LiteMath::normalize(LiteMath::float3(0.0f) - m_direction); } 
    LiteMath::float3 getColor() override{ return m_color; };
    float getDistanceFrom(const LiteMath::float3&) override { return std::numeric_limits<float>::infinity(); };

    LiteMath::float3 m_color;
    LiteMath::float3 m_direction;
};

struct PointLight: public LightInfo {
public:
    PointLight(LiteMath::float3 color, LiteMath::float3 position)
        : m_color(color)
        , m_position(position) {}

    LiteMath::float3 getDirectionFrom(const LiteMath::float3& from) override { return LiteMath::normalize(m_position - from); } 
    LiteMath::float3 getColor() override { return m_color; }; 
    float getDistanceFrom(const LiteMath::float3& point) override {
        return LiteMath::length(point - m_position);
    }

    LiteMath::float3 m_color;
    LiteMath::float3 m_position;
};

