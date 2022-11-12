#include "explosion.hpp"
#include "utility.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include <unordered_set>

namespace sand {

auto explosion_ray(
    world& pixels,
    glm::vec2 start,
    glm::vec2 end,
    const explosion& info
)
    -> void
{
    // Calculate a step length small enough to hit every pixel on the path.
    const auto line = end - start;
    const auto step = line / glm::max(glm::abs(line.x), glm::abs(line.y));

    auto curr = start;

    const auto blast_limit = random_from_range(info.min_radius, info.max_radius);
    while (pixels.valid(curr) && glm::length2(curr - start) < glm::pow(blast_limit, 2)) {
        if (pixels.at(curr).type == pixel_type::titanium) {
            break;
        }
        pixels.set(curr, random_unit() < 0.05f ? pixel::ember() : pixel::air());
        curr += step;
    }
    
    const auto scorch_limit = glm::length(curr - start) + std::abs(random_normal(0.0f, info.scorch));
    while (pixels.valid(curr) && glm::length2(curr - start) < glm::pow(scorch_limit, 2)) {
        if (properties(pixels.at(curr)).phase == pixel_phase::solid) {
            pixels.at(curr).colour *= 0.8f;
        }
        curr += step;
    }
}

auto apply_explosion(world& pixels, glm::vec2 pos, const explosion& info) -> void
{
    const auto boundary = info.max_radius + 3 * info.scorch;

    for (int i = -boundary; i != boundary + 1; ++i) {
        explosion_ray(pixels, pos, pos + glm::vec2{i, boundary}, info);
        explosion_ray(pixels, pos, pos + glm::vec2{i, -boundary}, info);
        explosion_ray(pixels, pos, pos + glm::vec2{boundary, i}, info);
        explosion_ray(pixels, pos, pos + glm::vec2{-boundary, i}, info);
    }
}

}