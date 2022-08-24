#pragma once
#include "pixel.hpp"
#include "serialise.hpp"

#include <cstdint>
#include <unordered_set>
#include <array>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace sand {

static constexpr std::uint32_t world_size  = 256;
static constexpr std::uint32_t chunk_size = 16;
static_assert(world_size % chunk_size == 0);

static constexpr std::uint32_t num_chunks = world_size / chunk_size;

struct chunk
{
    bool should_step      = true;
    bool should_step_next = true;
};

class world
{
public:
    using pixels = std::array<pixel, world_size * world_size>;
    using chunks = std::array<chunk, num_chunks * num_chunks>;

private:
    pixels d_pixels;
    chunks d_chunks;

public:
    world();

    // Returns true if the given position exists and false otherwise
    auto valid(glm::ivec2 pos) const -> bool;
    
    auto simulate() -> void;

    auto set(glm::ivec2 pos, const pixel& p) -> void;
    auto fill(const pixel& p) -> void;

    auto at(glm::ivec2 pos) const -> const pixel&;
    auto at(glm::ivec2 pos) -> pixel&;

    // Returns the rhs
    auto swap(glm::ivec2 lhs, glm::ivec2 rhs) -> glm::ivec2;

    // Chunk API
    auto wake_chunk_with_pixel(glm::ivec2 pixel) -> void;
    auto wake_all_chunks() -> void;
    auto num_awake_chunks() const -> std::size_t;
    auto is_chunk_awake(glm::ivec2 pixel) const -> bool;

    auto serialise(auto& archive) -> void
    {
        archive(d_pixels);
    }
};

}