#include "shader.hpp"
#include "utility.hpp"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <filesystem>
#include <fstream>
#include <print>

namespace sand {
namespace {

auto compile_shader(std::uint32_t type, const std::string& source) -> std::uint32_t
{
	std::uint32_t id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);

	int result = 0;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (!result) {
		char infoLog[512];
    	glGetShaderInfoLog(id, 512, NULL, infoLog);
        std::print("[{} ERROR] {}\n", (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT", infoLog);
		glDeleteShader(id);
		std::terminate();
	}

	return id;
}

}

auto parse_shader(const std::filesystem::path& file) -> std::string
{
	if (!std::filesystem::exists(file)) {
		print("FATAL: Shader file '{}' does not exist!\n", file.string());
		std::terminate();
	}
	std::ifstream stream(file);
	std::string shader((std::istreambuf_iterator<char>(stream)),
		                std::istreambuf_iterator<char>());
	return shader;
}

shader::shader(const std::filesystem::path& vertex_shader,
               const std::filesystem::path& fragment_shader
)
    : shader(parse_shader(vertex_shader), parse_shader(fragment_shader))
{
}

shader::shader(const std::string& vertex_shader_source,
		       const std::string& fragment_shader_source)
	: d_program(glCreateProgram())
	, d_vertex_shader(compile_shader(GL_VERTEX_SHADER, vertex_shader_source))
	, d_fragment_shader(compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source))
{
	glAttachShader(d_program, d_vertex_shader);
	glAttachShader(d_program, d_fragment_shader);
	glLinkProgram(d_program);
	glValidateProgram(d_program);
}

auto shader::get_location(const std::string& name) const -> std::uint32_t
{
    return glGetUniformLocation(d_program, name.c_str());
}

auto shader::bind() const -> void
{
    glUseProgram(d_program);
}

auto shader::unbind() const -> void
{
    glUseProgram(0);
}

auto shader::load_mat4(const std::string& name, const glm::mat4& matrix) const -> void
{
    glUniformMatrix4fv(get_location(name), 1, GL_FALSE, glm::value_ptr(matrix));
}

auto shader::load_vec2(const std::string& name, const glm::vec2& vector) const -> void
{
	glUniform2f(get_location(name), vector.x, vector.y);
}

auto shader::load_vec3(const std::string& name, const glm::vec3& vector) const -> void
{
	glUniform3f(get_location(name), vector.x, vector.y, vector.z);
}

auto shader::load_vec4(const std::string& name, const glm::vec4& vector) const -> void
{
	glUniform4f(get_location(name), vector.x, vector.y, vector.z, vector.w);
}

auto shader::load_sampler(const std::string& name, int value) const -> void
{
	glProgramUniform1i(d_program, get_location(name), value);
}

auto shader::load_int(const std::string& name, int value) const -> void
{
	glProgramUniform1i(d_program, get_location(name), value);
}

auto shader::load_float(const std::string& name, float value) const -> void
{
	glUniform1f(get_location(name), value);
}

}