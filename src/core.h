#ifndef LIMINAL_RENDERER_CORE_H
#define LIMINAL_RENDERER_CORE_H

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace liminal {

static const float kPi = 3.14159265358979323846f;
static const float kEpsilon = 0.0001f;
static const float kHuge = 1.0e30f;

struct Vec3 {
    float x;
    float y;
    float z;

    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    explicit Vec3(float value) : x(value), y(value), z(value) {}
    Vec3(float vx, float vy, float vz) : x(vx), y(vy), z(vz) {}
};

inline Vec3 operator+(const Vec3& a, const Vec3& b)
{
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline Vec3 operator-(const Vec3& a, const Vec3& b)
{
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline Vec3 operator-(const Vec3& value)
{
    return Vec3(-value.x, -value.y, -value.z);
}

inline Vec3 operator*(const Vec3& value, float scalar)
{
    return Vec3(value.x * scalar, value.y * scalar, value.z * scalar);
}

inline Vec3 operator*(const Vec3& a, const Vec3& b)
{
    return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

inline Vec3 operator*(float scalar, const Vec3& value)
{
    return value * scalar;
}

inline Vec3 operator/(const Vec3& value, float scalar)
{
    return Vec3(value.x / scalar, value.y / scalar, value.z / scalar);
}

inline Vec3& operator+=(Vec3& a, const Vec3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline Vec3& operator*=(Vec3& value, float scalar)
{
    value.x *= scalar;
    value.y *= scalar;
    value.z *= scalar;
    return value;
}

inline Vec3& operator*=(Vec3& a, const Vec3& b)
{
    a.x *= b.x;
    a.y *= b.y;
    a.z *= b.z;
    return a;
}

inline Vec3& operator/=(Vec3& value, float scalar)
{
    value.x /= scalar;
    value.y /= scalar;
    value.z /= scalar;
    return value;
}

inline float Dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

inline float LengthSquared(const Vec3& value)
{
    return Dot(value, value);
}

inline float Length(const Vec3& value)
{
    return sqrtf(LengthSquared(value));
}

inline Vec3 Normalize(const Vec3& value)
{
    const float length = Length(value);
    if (length <= kEpsilon) {
        return Vec3(0.0f, 1.0f, 0.0f);
    }
    return value / length;
}

inline Vec3 MinVec(const Vec3& a, const Vec3& b)
{
    return Vec3(
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::min(a.z, b.z));
}

inline Vec3 MaxVec(const Vec3& a, const Vec3& b)
{
    return Vec3(
        std::max(a.x, b.x),
        std::max(a.y, b.y),
        std::max(a.z, b.z));
}

inline float Clamp(float value, float min_value, float max_value)
{
    return std::max(min_value, std::min(value, max_value));
}

inline float Luminance(const Vec3& rgb)
{
    return rgb.x * 0.2126f + rgb.y * 0.7152f + rgb.z * 0.0722f;
}

inline float MaxComponent(const Vec3& value)
{
    return std::max(value.x, std::max(value.y, value.z));
}

inline bool IsNearBlack(const Vec3& value)
{
    return value.x <= kEpsilon && value.y <= kEpsilon && value.z <= kEpsilon;
}

enum MaterialSemantic {
    kMaterialSemanticNeutral = 0,
    kMaterialSemanticDesert,
    kMaterialSemanticCactus,
    kMaterialSemanticRackLed,
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

struct Aabb {
    Vec3 min;
    Vec3 max;

    Aabb() : min(kHuge), max(-kHuge) {}

    void Expand(const Vec3& point)
    {
        min = MinVec(min, point);
        max = MaxVec(max, point);
    }

    void Expand(const Aabb& other)
    {
        Expand(other.min);
        Expand(other.max);
    }
};

struct Material {
    std::string name;
    Vec3 albedo;
    Vec3 emission;
    MaterialSemantic semantic;

    Material()
        : albedo(0.6f)
        , emission(0.0f)
        , semantic(kMaterialSemanticNeutral)
    {
    }
};

struct Triangle {
    Vec3 v0;
    Vec3 v1;
    Vec3 v2;
    Vec3 normal;
    Vec3 centroid;
    Aabb bounds;
    int material_index;
    float area;

    Triangle() : material_index(0), area(0.0f) {}
};

struct Hit {
    float t;
    int triangle_index;
    Vec3 position;
    Vec3 normal;

    Hit() : t(kHuge), triangle_index(-1) {}
};

struct Camera {
    Vec3 eye;
    Vec3 target;
    Vec3 up;
    float vertical_fov_degrees;

    Camera()
        : eye(278.0f, 273.0f, -300.0f)
        , target(278.0f, 273.0f, 0.0f)
        , up(0.0f, 1.0f, 0.0f)
        , vertical_fov_degrees(40.0f)
    {
    }
};

struct CameraSpotlight {
    bool enabled;
    float panel_width;
    float panel_height;
    Vec3 local_offset;
    float range;
    float cone_inner_degrees;
    float cone_outer_degrees;
    float intensity;

    CameraSpotlight()
        : enabled(false)
        , panel_width(1.0f)
        , panel_height(1.0f)
        , local_offset(0.0f, 0.0f, 0.35f)
        , range(12.0f)
        , cone_inner_degrees(16.0f)
        , cone_outer_degrees(36.0f)
        , intensity(120.0f)
    {
    }
};

struct SkyBackground {
    bool enabled;
    float zenith_luminance;
    float horizon_luminance;
    float nadir_luminance;
    float horizon_band;
    float horizon_curve;
    float noise_amount;
    float star_density;
    float star_intensity;
    float star_radius;
    uint32_t seed;

    SkyBackground()
        : enabled(false)
        , zenith_luminance(0.02f)
        , horizon_luminance(0.18f)
        , nadir_luminance(0.01f)
        , horizon_band(0.22f)
        , horizon_curve(1.65f)
        , noise_amount(0.10f)
        , star_density(0.0018f)
        , star_intensity(0.90f)
        , star_radius(0.075f)
        , seed(1u)
    {
    }
};

struct RenderConfig {
    int width;
    int height;
    int samples_per_pixel;
    int max_bounces;
    int direct_light_samples;
    uint32_t seed;
    float exposure;

    RenderConfig()
        : width(800)
        , height(400)
        , samples_per_pixel(16)
        , max_bounces(3)
        , direct_light_samples(2)
        , seed(1337u)
        , exposure(1.0f)
    {
    }
};

class Rng {
public:
    explicit Rng(uint32_t seed_value)
        : state_(seed_value ? seed_value : 0x12345678u)
    {
    }

    uint32_t NextU32()
    {
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }

    float NextFloat()
    {
        const uint32_t bits = NextU32() >> 8;
        return static_cast<float>(bits) / static_cast<float>(0x01000000u);
    }

private:
    uint32_t state_;
};

}  // namespace liminal

#endif
