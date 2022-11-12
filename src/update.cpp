#include "update.hpp"
#include "utility.hpp"
#include "config.hpp"

#include <array>
#include <utility>
#include <variant>
#include <algorithm>
#include <random>
#include <ranges>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace sand {
namespace {

static constexpr auto neighbour_offsets = std::array{
    glm::ivec2{1, 0},
    glm::ivec2{-1, 0},
    glm::ivec2{0, 1},
    glm::ivec2{0, -1},
    glm::ivec2{1, 1},
    glm::ivec2{-1, -1},
    glm::ivec2{-1, 1},
    glm::ivec2{1, -1}
};

auto can_pixel_move_to(const world& pixels, glm::ivec2 src_pos, glm::ivec2 dst_pos) -> bool
{
    if (!pixels.valid(src_pos) || !pixels.valid(dst_pos)) { return false; }

    // If the destination is empty, we can always move there
    if (pixels.at(dst_pos).type == pixel_type::none) { return true; }

    const auto src = properties(pixels.at(src_pos)).phase;
    const auto dst = properties(pixels.at(dst_pos)).phase;

    using pm = pixel_phase;
    switch (src) {
        case pm::solid:
            return dst == pm::liquid
                || dst == pm::gas;

        case pm::liquid:
            return dst == pm::gas;

        default:
            return false;
    }
}

auto set_adjacent_free_falling(world& pixels, glm::ivec2 pos) -> void
{
    const auto l = pos + glm::ivec2{-1, 0};
    const auto r = pos + glm::ivec2{1, 0};

    for (const auto x : {l, r}) {
        if (pixels.valid(x)) {
            auto& px = pixels.at(x);
            const auto& props = properties(px);
            if (props.gravity_factor != 0.0f) {
                pixels.wake_chunk_with_pixel(l);
                if (random_unit() > props.inertial_resistance) px.flags[is_falling] = true;
            }
        }
    }
}

// Moves towards the given offset, updating pos to the new postion and returning
// true if the position has changed
auto move_offset(world& pixels, glm::ivec2& pos, glm::ivec2 offset) -> bool
{
    glm::ivec2 start_pos = pos;

    const auto a = pos;
    const auto b = pos + offset;
    const auto steps = glm::max(glm::abs(a.x - b.x), glm::abs(a.y - b.y));

    for (int i = 0; i != steps; ++i) {
        const auto next_pos = a + (b - a) * (i + 1)/steps;

        if (!can_pixel_move_to(pixels, pos, next_pos)) {
            break;
        }

        pos = pixels.swap(pos, next_pos);
        set_adjacent_free_falling(pixels, pos);
    }

    if (start_pos != pos) {
        pixels.at(pos).flags[is_falling] = true;
        pixels.wake_chunk_with_pixel(pos);
        return true;
    }

    return false;
}

auto is_surrounded(const world& pixels, glm::ivec2 pos) -> bool
{ 
    for (const auto& offset : neighbour_offsets) {
        if (pixels.valid(pos + offset)) {
            if (pixels.at(pos + offset).type == pixel_type::none) {
                return false;
            }
        }
    }
    return true;
}

auto sign(float f) -> int
{
    if (f < 0.0f) return -1;
    if (f > 0.0f) return 1;
    return 0;
}

inline auto update_pixel_position(world& pixels, glm::ivec2& pos) -> void
{
    auto& data = pixels.at(pos);
    const auto& props = properties(data);
    const auto start_pos = pos;

    // Pixels that don't move have their is_falling flag set to false at the end
    const auto after_position_update = scope_exit{[&] {
        const auto falling = pos != start_pos;
        pixels.at(pos).flags[is_falling] = falling;
    }};

    // Apply gravity
    if (props.gravity_factor) {
        const auto gravity_factor = props.gravity_factor;
        data.velocity += gravity_factor * config::gravity * config::time_step;
        if (move_offset(pixels, pos, data.velocity)) return;
    }

    // If we have resistance to moving and we are not, then we are not moving
    if (props.inertial_resistance && !pixels.at(pos).flags[is_falling]) {
        return;
    }

    // Attempts to move diagonally up/down
    if (props.can_move_diagonally) {
        const auto dir = sign(props.gravity_factor);
        auto offsets = std::array{glm::ivec2{-1, dir}, glm::ivec2{1, dir}};
        if (coin_flip()) std::swap(offsets[0], offsets[1]);

        for (auto offset : offsets) {
            if (move_offset(pixels, pos, offset)) return;
        }
        data.velocity.y = 0.0f;
    }

    // Attempts to disperse outwards according to the dispersion rate
    if (props.dispersion_rate) {
        data.velocity.y = 0.0f;

        const auto dr = props.dispersion_rate;
        auto offsets = std::array{glm::ivec2{-dr, 0}, glm::ivec2{dr, 0}};
        if (coin_flip()) std::swap(offsets[0], offsets[1]);

        for (auto offset : offsets) {
            if (move_offset(pixels, pos, offset)) return;
        }
    }
}

// Update logic for single pixels depending on properties only
inline auto update_pixel_attributes(world& pixels, glm::ivec2 pos) -> void
{
    auto& pixel = pixels.at(pos);
    const auto& props = properties(pixel);

    // If a pixel is burning, keep the chunk awake
    if (pixel.flags[is_burning]) {
        pixels.wake_chunk_with_pixel(pos);
    }

    // is_burning status
    if (pixel.flags[is_burning]) {

        // First, see if it can be put out
        const auto put_out = is_surrounded(pixels, pos) ? props.put_out_surrounded : props.put_out;
        if (random_unit() < put_out) {
            pixel.flags[is_burning] = false;
        }

        // Second, see if it gets destroyed
        if (random_unit() < props.burn_out_chance) {
            pixel = pixel::air();
        }
    }
}

inline auto affect_neighbours(world& pixels, glm::ivec2 pos) -> void
{
    auto& pixel = pixels.at(pos);
    const auto& props = properties(pixel);

    for (const auto& offset : neighbour_offsets) {
        if (pixels.valid(pos + offset)) {
            const auto neigh_pos = pos + offset;
            auto& neighbour = pixels.at(neigh_pos);

            // Boil water
            if (props.can_boil_water) {
                if (neighbour.type == pixel_type::water) {
                    neighbour = pixel::steam();
                }
            }

            // Corrode neighbours
            if (props.is_corrosion_source) {
                if (random_unit() > properties(neighbour).corrosion_resist) {
                    neighbour = pixel::air();
                    if (random_unit() > 0.9f) {
                        pixel = pixel::air();
                    }
                }
            }
            
            // Spread fire
            if (props.is_burn_source || pixel.flags[is_burning]) {
                if (random_unit() < properties(neighbour).flammability) {
                    neighbour.flags[is_burning] = true;
                    pixels.wake_chunk_with_pixel(neigh_pos);
                }
            }

            // Produce embers
            const bool can_produce_embers = props.is_ember_source || pixel.flags[is_burning];
            if (can_produce_embers && neighbour.type == pixel_type::none) {
                if (random_unit() < 0.01f) {
                    neighbour = pixel::ember();
                    pixels.wake_chunk_with_pixel(neigh_pos);
                }
            }
        }
    }
}

}

auto update_pixel(world& pixels, glm::ivec2 pos) -> void
{
    if (pixels.at(pos).type == pixel_type::none || pixels.at(pos).flags[is_updated]) {
        return;
    }

    update_pixel_position(pixels, pos);
    update_pixel_attributes(pixels, pos);

    affect_neighbours(pixels, pos);

    pixels.at(pos).flags[is_updated] = true;
}

auto explosion_ray(
    world& pixels,
    std::unordered_set<glm::ivec2>& checked,
    glm::ivec2 pos,
    glm::ivec2 end
)
    -> void
{
    const auto a = pos;
    const auto b = end;
    const auto steps = glm::max(glm::abs(a.x - b.x), glm::abs(a.y - b.y));

    for (int i = 0; i < steps; ++i) {
        const auto curr = a + (b - a) * i/steps;
        if (checked.contains(curr)) {
            continue;
        }
        if (!pixels.valid(curr)) {
            return;
        }

        if (pixels.at(curr).type == pixel_type::titanium) {
            return;
        } else {
            pixels.set(curr, random_unit() < 0.05f ? pixel::ember() : pixel::air());
            checked.emplace(curr);
        }
    }
}

auto apply_explosion(world& pixels, glm::ivec2 pos, float radius, float strenth) -> void
{
    std::unordered_set<glm::ivec2> checked;
    for (int x = -radius; x < radius; ++x) {
        for (int y = -radius; y < radius; ++y) {
            auto offset = glm::vec2{x, y};
            if (glm::length2(offset) > radius * radius) {
                offset *= radius/glm::length(offset);
            }
            explosion_ray(pixels, checked, pos, pos + glm::ivec2{offset});
        }
    }
}

}