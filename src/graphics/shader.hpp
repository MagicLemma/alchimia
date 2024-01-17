#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <filesystem>

namespace sand {

std::string parse_shader(const std::string& filepath);

class shader
{
    std::uint32_t d_program;
    std::uint32_t d_vertex_shader;
    std::uint32_t d_fragment_shader;

    std::uint32_t get_location(const std::string& name) const;

public:
    shader(const std::filesystem::path& vertex_shader,
           const std::filesystem::path& fragment_shader);

    shader(const std::string& vertex_shader_source,
           const std::string& fragment_shader_source);

    auto bind() const -> void;
    auto unbind() const -> void;

    auto load_mat4(const std::string& name, const glm::mat4& matrix) const -> void;
    auto load_vec2(const std::string& name, const glm::vec2& vector) const -> void;
    auto load_vec3(const std::string& name, const glm::vec3& vector) const -> void;
    auto load_vec4(const std::string& name, const glm::vec4& vector) const -> void;
    auto load_int(const std::string& name, int value) const -> void;
    auto load_float(const std::string& name, float value) const -> void;
    auto load_sampler(const std::string& name, int value) const -> void;
};

}