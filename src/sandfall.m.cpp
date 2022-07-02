#include "log.h"
#include "tile.h"
#include "pixel.h"
#include "world_settings.h"
#include "timer.hpp"
#include "random.hpp"

#include "graphics/window.h"
#include "graphics/shader.h"
#include "graphics/texture.hpp"
#include "graphics/ui.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui/imgui.h>

#include <cstdint>
#include <cstddef>
#include <array>
#include <utility>
#include <memory>
#include <chrono>
#include <random>
#include <numbers>
#include <string_view>

constexpr glm::vec4 BACKGROUND = { 44.0f / 256.0f, 58.0f / 256.0f, 71.0f / 256.0f, 1.0 };

struct pixel_type_loop
{
    int type = 0;

    auto get_pixel() -> sand::pixel
    {
        switch (type) {
            case 0: return sand::pixel::air();
            case 1: return sand::pixel::sand();
            case 2: return sand::pixel::coal();
            case 3: return sand::pixel::water();
            case 4: return sand::pixel::rock();
            case 5: return sand::pixel::red_sand();
            default: return sand::pixel::air();
        }
    }

    auto get_pixel_name() -> std::string_view
    {
        switch (type) {
            case 0: return "air";
            case 1: return "sand";
            case 2: return "coal";
            case 3: return "water";
            case 4: return "rock";
            case 5: return "red_sand";
            default: return "unknown";
        }
    }
};

float random_from_range(float min, float max)
{
    static std::default_random_engine gen;
    return std::uniform_real_distribution(min, max)(gen);
}

auto circle_offset(float radius) -> glm::ivec2
{
    const auto r = random_from_range(0, radius);
    const auto theta = random_from_range(0, 2 * std::numbers::pi);
    return { r * std::cos(theta), r * std::sin(theta) };
}

int main()
{
    using namespace sand;

    auto window = sand::window{"sandfall", 1280, 720};
    
    auto settings = sand::world_settings{
        .gravity = {0.0f, 9.81f}
    };

    float size = 720.0f;
    float vertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        size, 0.0f, 1.0f, 0.0f,
        size, size, 1.0f, 1.0f,
        0.0f, size, 0.0f, 1.0f
    };

    std::uint32_t indices[] = {0, 1, 2, 0, 2, 3};

    auto VAO = std::uint32_t{};
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    auto VBO = std::uint32_t{};
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    auto EBO = std::uint32_t{};
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    auto loop = pixel_type_loop{};
    auto left_mouse_down = false; // TODO: Remove, do it in a better way

    auto ui = sand::ui{window};

    window.set_callback([&](sand::event& event) {
        auto& io = ImGui::GetIO();
        if (event.is_keyboard_event() && io.WantCaptureKeyboard) {
            event.consume();
            return;
        }
        if (event.is_mount_event() && io.WantCaptureMouse) {
            event.consume();
            return;
        }

        if (auto e = event.get_if<sand::mouse_pressed_event>()) {
            switch (e->button) {
                case 0: left_mouse_down = true; return;
            }
        }
        else if (auto e = event.get_if<sand::mouse_released_event>()) {
            switch (e->button) {
                case 0: left_mouse_down = false; return;
            }
        }
        else if (auto e = event.get_if<sand::mouse_scrolled_event>()) {
            if (e->y_offset > 0) {
                ++loop.type;
            } else if (e->y_offset < 0) {
                --loop.type;
            }
        }
    });

    auto tile = std::make_unique<sand::tile>();
    auto shader = sand::shader{"res\\vertex.glsl", "res\\fragment.glsl"};
    auto texture = sand::texture{sand::tile_size, sand::tile_size};

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    shader.bind();
    shader.load_sampler("u_texture", 0);
    shader.load_mat4("u_proj_matrix", glm::ortho(0.0f, window.width(), window.height(), 0.0f));
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    auto frame_length = 1.0 / 60.0;
    auto accumulator = 0.0;
    auto timer = sand::timer{};

    while (window.is_running()) {
        const double dt = timer.on_update();
        window.poll_events();

        ui.begin_frame();

        //bool show = true;
        //ImGui::ShowDemoWindow(&show);

        if (ImGui::Begin("Editor")) {

        }
        ImGui::End();

        accumulator += dt;
        bool updated = false;
        while (accumulator > frame_length) {
            tile->simulate(settings, frame_length);
            accumulator -= frame_length;
            updated = true;

        }

        if (updated) {
            texture.set_data(tile->data());
            window.set_name(fmt::format("Sandfall - Current tool: {} [FPS: {}]", loop.get_pixel_name(), timer.frame_rate()));
        }

        window.clear();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        
        if (left_mouse_down) {
            const auto coord = circle_offset(10.0f) + glm::ivec2((sand::tile_size_f / size) * window.get_mouse_pos());
            if (tile->valid(coord)) {
                tile->set(coord, loop.get_pixel());
            }
        }

        ui.end_frame();

        window.swap_buffers();
    }
}