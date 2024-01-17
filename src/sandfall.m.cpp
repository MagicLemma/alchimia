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

class PlayerController : public b2ContactListener {
public:
    PlayerController(b2World& world) : world(world) {
        // Create player body
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(200.0f, 100.0f);
        playerBody = world.CreateBody(&bodyDef);

        b2PolygonShape dynamicBox;
        dynamicBox.SetAsBox(5.0f, 10.0f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &dynamicBox;
        fixtureDef.density = 5.0f;
        fixtureDef.friction = 0.3f;
        playerBody->CreateFixture(&fixtureDef);
        playerBody->SetFixedRotation(true);

        // Set up contact listener
        world.SetContactListener(this);
    }

    void handleInput(sand::keyboard& k) {
        // Move left
        if (k.is_down(sand::keyboard_key::A)) {
            playerBody->ApplyForceToCenter(b2Vec2(-50000.0f, 0.0f), true);
        }

        // Move right
        if (k.is_down(sand::keyboard_key::D)) {
            playerBody->ApplyForceToCenter(b2Vec2(50000.0f, 0.0f), true);
        }

        // Jump
        if (k.is_down_this_frame(sand::keyboard_key::W) && onGround) {
            playerBody->ApplyLinearImpulse(b2Vec2(0.0f, -50000.0f), playerBody->GetWorldCenter(), true);
            onGround = false;
        }
    }

    void Step(float timeStep) {
        world.Step(timeStep, 8, 3);
    }

    // Contact listener functions
    void BeginContact(b2Contact* contact) {
        // Check if player is on the ground
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();

        if (fixtureA == playerBody->GetFixtureList() || fixtureB == playerBody->GetFixtureList()) {
            onGround = true;
        }
    }

    auto rect() const {
        return playerBody->GetPosition();
    }

    auto angle() const {
        return playerBody->GetAngle();
    }

private:
    b2World& world;
    b2Body* playerBody;
    bool onGround = false;
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

    // Create ground body (solid line segment)
    {
        b2BodyDef groundBodyDef;
        groundBodyDef.position.Set(0.0f, 0.0f);
        b2Body* groundBody = physics.CreateBody(&groundBodyDef);

        b2Vec2 point1(0.0f, 256.0f); // Replace with your actual coordinates
        b2Vec2 point2(256.0f, 256.0f); // Replace with your actual coordinates

        b2EdgeShape edgeShape{};
        edgeShape.SetTwoSided(point1, point2);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;

        groundBody->CreateFixture(&fixtureDef);
    }

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

    auto world       = std::make_unique<sand::world>();
    auto renderer    = sand::renderer{};
    auto ui          = sand::ui{window};
    auto accumulator = 0.0;
    auto timer       = sand::timer{};
    auto p_renderer  = sand::player_renderer{};
    PlayerController playerController(physics);

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
            playerController.handleInput(keyboard);
            playerController.Step(sand::config::time_step);
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
        
        // Next, draw the editor UI
        ui.begin_frame();
        if (display_ui(editor, *world, timer, window, camera)) updated = true;

        // Draw the world
        renderer.bind();
        if (updated) {
            renderer.update(*world, editor.show_chunks, camera);
        }
        renderer.draw();

        const auto player_pos = playerController.rect();
        p_renderer.bind();
        p_renderer.draw(*world, {player_pos.x, player_pos.y, 10.0f, 20.0f}, playerController.angle(), glm::vec3{0.0, 1.0, 0.0}, camera);
        p_renderer.draw(*world, {128.0, 266.0, 256.0f, 20.0f}, 0, glm::vec3{1.0, 1.0, 0.0}, camera);

        ui.end_frame();

        window.swap_buffers();
    }
    
    return 0;
}