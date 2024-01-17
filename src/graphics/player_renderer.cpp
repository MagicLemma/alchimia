#include "player_renderer.hpp"
#include "utility.hpp"
#include "pixel.hpp"
#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace sand {
namespace {

constexpr auto vertex_shader = R"SHADER(
#version 410 core
layout (location = 0) in vec2 p_position;

uniform vec4  u_rect;
uniform float u_angle;
uniform mat4  u_proj_matrix;

out vec2 pass_uv;

mat2 rotate(float theta)
{
    float c = cos(theta);
    float s = sin(theta);
    return mat2(c, s, -s, c);
}

void main()
{
    vec2 position = u_rect.xy;
    vec2 dimensions = u_rect.zw / 2;

    vec2 screen_position = rotate(u_angle) * (p_position * dimensions) + position;

    pass_uv = p_position;
    gl_Position = u_proj_matrix * vec4(screen_position, 0, 1);
}
)SHADER";

constexpr auto fragment_shader = R"SHADER(
#version 410 core
layout (location = 0) out vec4 out_colour;

in vec2 pass_uv;

uniform sampler2D u_texture;
uniform vec3  u_colour;

void main()
{
    out_colour = vec4(u_colour, 1.0);
}
)SHADER";

}

player_renderer::player_renderer()
    : d_vao{0}
    , d_vbo{0}
    , d_ebo{0}
    , d_shader{std::string{vertex_shader}, std::string{fragment_shader}}
{
    const float vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    const std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &d_vao);
    glBindVertexArray(d_vao);

    glGenBuffers(1, &d_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, d_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &d_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, d_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    d_shader.bind();
}

player_renderer::~player_renderer()
{
    glDeleteBuffers(1, &d_ebo);
    glDeleteBuffers(1, &d_vbo);
    glDeleteVertexArrays(1, &d_vao);
}

auto player_renderer::bind() const -> void
{
    glBindVertexArray(d_vao);
    d_shader.bind();
}

auto player_renderer::draw(const world& world, glm::vec4 d, float angle, glm::vec3 colour, const camera& camera) -> void
{
    d_shader.load_vec4("u_rect", d);
    d_shader.load_float("u_angle", angle);
    d_shader.load_vec3("u_colour", colour);

    const auto dimensions = glm::vec2{camera.screen_width, camera.screen_height} / camera.world_to_screen;
    const auto projection = glm::ortho(0.0f, dimensions.x, dimensions.y, 0.0f);
    d_shader.load_mat4("u_proj_matrix", glm::translate(projection, glm::vec3{-camera.top_left, 0.0f}));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

}