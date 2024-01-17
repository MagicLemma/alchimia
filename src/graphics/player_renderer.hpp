#pragma once
#include "graphics/texture.hpp"
#include "graphics/shader.hpp"
#include "world.hpp"
#include "player.hpp"
#include "camera.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <array>

namespace sand {

// Responsible for rendering the world to the screen.
class player_renderer
{
    std::uint32_t d_vao;
    std::uint32_t d_vbo;
    std::uint32_t d_ebo;

    shader d_shader;

    player_renderer(const player_renderer&) = delete;
    player_renderer& operator=(const player_renderer&) = delete;

public:
    player_renderer();
    ~player_renderer();

    auto bind() const -> void;

    auto draw(const world& world, glm::vec4 dimensions, float angle, glm::vec3 colour, const camera& camera) -> void;
};

}