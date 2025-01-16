#include "world.hpp"
#include "pixel.hpp"
#include "config.hpp"
#include "utility.hpp"
#include "editor.hpp"
#include "camera.hpp"
#include "update.hpp"
#include "explosion.hpp"
#include "mouse.hpp"
#include "player.hpp"

#include "graphics/renderer.hpp"
#include "graphics/player_renderer.hpp"
#include "graphics/window.hpp"
#include "graphics/ui.hpp"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <box2d/box2d.h>

#include <memory>
#include <print>

// Converts a point in pixel space to world space
auto pixel_to_physics(glm::vec2 px) -> b2Vec2
{
    b2Vec2 pos(px.x / sand::config::pixels_per_meter, px.y / sand::config::pixels_per_meter);
    return pos;
}

// Converts a point in world space to pixel space
auto physics_to_pixel(b2Vec2 px) -> glm::vec2
{
    glm::vec2 pos(px.x * sand::config::pixels_per_meter, px.y * sand::config::pixels_per_meter);
    return pos;
}

class player_controller {
    int     d_width;
    int     d_height;
    b2Body* d_playerBody = nullptr;
    bool    d_doubleJump = false;

public:
    // width and height are in pixel space
    player_controller(b2World& world, int width, int height)
        : d_width{width}
        , d_height{height}
    {
        // Create player body
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        const auto position = pixel_to_physics({200.0f, 100.0f});
        const auto dimensions = pixel_to_physics({10, 20});
        bodyDef.position.Set(position.x, position.y);
        d_playerBody = world.CreateBody(&bodyDef);
        d_playerBody->SetFixedRotation(true);

        {
            b2PolygonShape dynamicBox;
            dynamicBox.SetAsBox(dimensions.x / 2, dimensions.y / 2);

            b2FixtureDef fixtureDef;
            fixtureDef.shape = &dynamicBox;
            fixtureDef.density = 12.0f;
            d_playerBody->CreateFixture(&fixtureDef);
        }

        // Slightly wider, slightly shorter, so the side have no friction
        // There must be a better way to do this.
        {
            b2PolygonShape dynamicBox;
            dynamicBox.SetAsBox(dimensions.x / 2 + 0.01, dimensions.y / 2 - 0.01);

            b2FixtureDef fixtureDef;
            fixtureDef.shape = &dynamicBox;
            fixtureDef.friction = 0.0;
            d_playerBody->CreateFixture(&fixtureDef);
        }
    }

    void handle_input(sand::keyboard& k) {
        // Move left
        if (k.is_down(sand::keyboard_key::A)) {
            const auto v = d_playerBody->GetLinearVelocity();
            d_playerBody->SetLinearVelocity({-3.0f, v.y});
        }

        // Move right
        if (k.is_down(sand::keyboard_key::D)) {
            const auto v = d_playerBody->GetLinearVelocity();
            d_playerBody->SetLinearVelocity({3.0f, v.y});
        }

        // Jump - need to check for collision below, this allows wall climbing
        bool onGround = false;
        for (auto edge = d_playerBody->GetContactList(); edge; edge = edge->next) {
            onGround = onGround || edge->contact->IsTouching();
        }
        if (onGround) { d_doubleJump = true; }
        
        if (k.is_down_this_frame(sand::keyboard_key::W)) {
            if (onGround || d_doubleJump) {
                if (!onGround) d_doubleJump = false;
                const auto v = d_playerBody->GetLinearVelocity();
                d_playerBody->SetLinearVelocity({v.x, -5.0f});
            }
        }
    }

    auto rect_pixels() const -> glm::vec4 {
        auto pos = physics_to_pixel(d_playerBody->GetPosition());
        return glm::vec4{pos.x, pos.y, d_width, d_height};
    }

    auto angle() const -> float {
        return d_playerBody->GetAngle();
    }
};

class static_physics_box
{
    int       d_width;
    int       d_height;
    glm::vec3 d_colour;
    b2Body*   d_body = nullptr;

public:
    static_physics_box(b2World& world, glm::vec2 pos, int width, int height, glm::vec3 colour)
        : d_width{width}
        , d_height{height}
        , d_colour{colour}
    {
        b2BodyDef bodyDef;
        bodyDef.type = b2_staticBody;
        const auto position = pixel_to_physics(pos);
        bodyDef.position.Set(position.x, position.y);
        d_body = world.CreateBody(&bodyDef);

        b2PolygonShape box;
        const auto dimensions = pixel_to_physics({width, height});
        box.SetAsBox(dimensions.x / 2, dimensions.y / 2);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &box;
        fixtureDef.friction = 1.0;
        d_body->CreateFixture(&fixtureDef);
    }

    auto rect_pixels() const -> glm::vec4 {
        auto pos = physics_to_pixel(d_body->GetPosition());
        return glm::vec4{pos.x, pos.y, d_width, d_height};
    }

    auto angle() const -> float {
        return d_body->GetAngle();
    }

    auto colour() const -> glm::vec3 {
        return d_colour;
    }
};

auto main() -> int
{
    auto exe_path = sand::get_executable_filepath().parent_path();
    std::print("Executable directory: {}\n", exe_path.string());
    auto window = sand::window{"sandfall", 1280, 720};
    auto editor = sand::editor{};
    auto mouse = sand::mouse{};
    auto keyboard = sand::keyboard{};

    auto gravity = b2Vec2{0.0f, 10.0f};
    auto physics = b2World{gravity};

    auto camera = sand::camera{
        .top_left = {0, 0},
        .screen_width = static_cast<float>(window.width()),
        .screen_height = static_cast<float>(window.height()),
        .world_to_screen = 720.0f / 256.0f
    };

    window.set_callback([&](const sand::event& event) {
        auto& io = ImGui::GetIO();
        if (event.is_keyboard_event() && io.WantCaptureKeyboard) {
            return;
        }
        if (event.is_mount_event() && io.WantCaptureMouse) {
            return;
        }

        mouse.on_event(event);
        keyboard.on_event(event);

        if (mouse.is_down(sand::mouse_button::right) && event.is<sand::mouse_moved_event>()) {
            const auto& e = event.as<sand::mouse_moved_event>();
            camera.top_left -= e.offset / camera.world_to_screen;
        }
        else if (event.is<sand::window_resize_event>()) {
            camera.screen_width = window.width();
            camera.screen_height = window.height();
        }
        else if (event.is<sand::mouse_scrolled_event>()) {
            const auto& e = event.as<sand::mouse_scrolled_event>();
            const auto old_centre = mouse_pos_world_space(window, camera);
            camera.world_to_screen += 0.1f * e.offset.y;
            camera.world_to_screen = std::clamp(camera.world_to_screen, 1.0f, 100.0f);
            const auto new_centre = mouse_pos_world_space(window, camera);
            camera.top_left -= new_centre - old_centre;
        }
    });

    auto world           = std::make_unique<sand::world>();
    auto world_renderer  = sand::renderer{};
    auto ui              = sand::ui{window};
    auto accumulator     = 0.0;
    auto timer           = sand::timer{};
    auto player_renderer = sand::player_renderer{};
    auto player          = player_controller(physics, 10, 20);
    
    auto floor = static_physics_box(physics, {128, 256 + 5}, 256, 10, {1.0, 1.0, 0.0});
    auto box = static_physics_box(physics, {64, 256 + 5}, 30, 50, {1.0, 1.0, 0.0});

    auto ground = std::vector<static_physics_box>{
        {physics, {128, 256 + 5}, 256, 10, {1.0, 1.0, 0.0}},
        {physics, {64, 256 + 5}, 30, 50, {1.0, 1.0, 0.0}},
        {physics, {130, 215}, 30, 10, {1.0, 1.0, 0.0}}
    };

    while (window.is_running()) {
        const double dt = timer.on_update();

        mouse.on_new_frame();
        keyboard.on_new_frame();
        
        window.poll_events();
        window.clear();

        accumulator += dt;
        bool updated = false;
        while (accumulator > sand::config::time_step) {
            sand::update(*world);
            player.handle_input(keyboard);
            physics.Step(sand::config::time_step, 8, 3);
            accumulator -= sand::config::time_step;
            updated = true;
        }

        const auto mouse_pos = pixel_at_mouse(window, camera);
        switch (editor.brush_type) {
            break; case 0:
                if (mouse.is_down(sand::mouse_button::left)) {
                    const auto coord = mouse_pos + sand::random_from_circle(editor.brush_size);
                    if (world->valid(coord)) {
                        world->set(coord, editor.get_pixel());
                        updated = true;
                    }
                }
            break; case 1:
                if (mouse.is_down(sand::mouse_button::left)) {
                    const auto half_extent = (int)(editor.brush_size / 2);
                    for (int x = mouse_pos.x - half_extent; x != mouse_pos.x + half_extent + 1; ++x) {
                        for (int y = mouse_pos.y - half_extent; y != mouse_pos.y + half_extent + 1; ++y) {
                            if (world->valid({x, y})) {
                                world->set({x, y}, editor.get_pixel());
                                updated = true;
                            }
                        }
                    }
                }
            break; case 2:
                if (mouse.is_down_this_frame(sand::mouse_button::left)) {
                    sand::apply_explosion(*world, mouse_pos, sand::explosion{
                        .min_radius = 40.0f, .max_radius = 45.0f, .scorch = 10.0f
                    });
                    updated = true;
                }
        }
        
        // Renders the UI but doesn't yet draw on the screen
        ui.begin_frame();
        if (display_ui(editor, *world, timer, window, camera)) {
            updated = true;
        }

        // Render and display the world
        world_renderer.bind();
        if (updated) {
            world_renderer.update(*world, editor.show_chunks, camera);
        }
        world_renderer.draw();

        // Render and display the player plus some temporary obstacles
        player_renderer.bind();
        player_renderer.draw(*world, player.rect_pixels(), player.angle(), glm::vec3{0.0, 1.0, 0.0}, camera);

        for (const auto& obj : ground) {
            player_renderer.draw(*world, obj.rect_pixels(), obj.angle(), obj.colour(), camera);
        }
        
        // Display the UI
        ui.end_frame();

        window.swap_buffers();
    }
    
    return 0;
}