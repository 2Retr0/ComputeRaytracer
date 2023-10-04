#pragma once

#include <string>
#include <utility>

#include "glm/gtx/hash.hpp"
#include "glm/vec3.hpp"

#define BAD_INDEX 0xFFFFFFFF

class RTMaterial {
public:
    enum Type : uint32_t {
        lambertian = 1,
        metal = 2,
        dielectric = 4,
        diffuseLight = 8,
    };

    struct GPU_t {
        glm::vec3 albedo;
        float fuzziness;
        glm::vec3 pad0;
        Type type;
        glm::vec3 pad1;
        uint32_t textureIndex {BAD_INDEX};

        bool operator==(const GPU_t &other) const {
            return albedo == other.albedo && fuzziness == other.fuzziness && type == other.type && textureIndex == other.textureIndex;
        }
    };

public:
    RTMaterial() = default;

    operator GPU_t() const { // NOLINT
        return material;
    }

    bool operator==(const RTMaterial &other) const {
        return material == other.material && texture == other.texture;
    };

public:
    std::string texture;
    uint32_t index {};
    GPU_t material {};

protected:
    explicit RTMaterial(Type type) {
        material.type = type;
    }
};

template<>
struct std::hash<RTMaterial> {
    uint64_t operator()(const RTMaterial &material) const noexcept {
        auto textureHash = std::hash<std::string>()(material.texture);
        auto albedoHash = std::hash<glm::vec3>()(material.material.albedo);
        auto hash = static_cast<uint32_t>(material.material.fuzziness) | material.material.type << 8;
        return textureHash ^ (albedoHash << 16) ^ (hash << 24);
    }
};


class Lambertian : public RTMaterial {
public:
    explicit Lambertian(const glm::vec3 &albedo) : RTMaterial(RTMaterial::Type::lambertian) {
        material.albedo = albedo;
    }

    explicit Lambertian(const char *texture) : RTMaterial(RTMaterial::Type::lambertian) {
        this->texture = texture;
    }
};


class Metal : public RTMaterial {
public:
    explicit Metal(const glm::vec3 &albedo, float fuzziness) : RTMaterial(RTMaterial::Type::metal) {
        material.albedo = albedo;
        material.fuzziness = fuzziness;
    }
};


class Dielectric : public RTMaterial {
public:
    explicit Dielectric(float refractiveIndex) : RTMaterial(RTMaterial::Type::dielectric) {
        material.fuzziness = refractiveIndex;
    }
};


class DiffuseLight : public RTMaterial {
public:
    explicit DiffuseLight(const glm::vec3 &albedo) : RTMaterial(RTMaterial::Type::diffuseLight) {
        material.albedo = albedo;
    }
};