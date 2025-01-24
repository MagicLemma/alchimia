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
#include "graphics/shape_renderer.hpp"
#include "graphics/window.hpp"
#include "graphics/ui.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <imgui/imgui.h>
#include <box2d/box2d.h>
#include <cereal/archives/binary.hpp>

#include <memory>
#include <print>
#include <fstream>
#include <cmath>

// Converts a point in pixel space to world space


class static_physics_box
{
    int       d_width;
    int       d_height;
    glm::vec4 d_colour;
    b2Body*   d_body = nullptr;

public:
    static_physics_box(b2World& world, glm::vec2 pos, int width, int height, glm::vec4 colour, float angle = 0.0f)
        : d_width{width}
        , d_height{height}
        , d_colour{colour}
    {
        b2BodyDef bodyDef;
        bodyDef.type = b2_staticBody;
        const auto position = sand::pixel_to_physics(pos);
        bodyDef.position.Set(position.x, position.y);
        bodyDef.angle = angle;
        d_body = world.CreateBody(&bodyDef);

        b2PolygonShape box;
        const auto dimensions = sand::pixel_to_physics({width, height});
        box.SetAsBox(dimensions.x / 2, dimensions.y / 2);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &box;
        fixtureDef.friction = 1.0;
        d_body->CreateFixture(&fixtureDef);
    }

    auto centre() const -> glm::vec2 {
        return sand::physics_to_pixel(d_body->GetPosition());;
    }

    auto width() const {
        return d_width;
    }

    auto height() const {
        return d_height;
    }

    auto angle() const -> float {
        return d_body->GetAngle();
    }

    auto colour() const -> glm::vec4 {
        return d_colour;
    }
};

static constexpr auto offsets = {glm::ivec2{-1, 0}, glm::ivec2{0, -1}, glm::ivec2{1, 0}, glm::ivec2{0, 1}};

auto flood_fill(const sand::world& w, int x, int y) -> std::unordered_set<glm::ivec2>
{
    std::unordered_set<glm::ivec2> ret;
    std::unordered_set<glm::ivec2> seen;
    std::vector<glm::ivec2> jobs;
    jobs.push_back({x, y});
    while (!jobs.empty()) {
        glm::ivec2 curr = jobs.back();
        jobs.pop_back();
        ret.insert(curr);
        ret.insert(curr + glm::ivec2(1, 0));
        ret.insert(curr + glm::ivec2(0, 1));
        ret.insert(curr + glm::ivec2(1, 1));
        seen.insert(curr);
        for (const auto offset : offsets) {
            const auto neighbour = curr + offset;
            if (!seen.contains(neighbour) && w.valid(neighbour) && w.at(neighbour).type != sand::pixel_type::none && !w.at(neighbour).flags.test(sand::pixel_flags::is_falling)) {
                jobs.push_back(neighbour);
            }
        }
    }
    return ret;
}

auto is_boundary(const sand::world& w, glm::ivec2 pos) -> bool
{
    for (const auto offset : offsets) {
        const auto n = pos + offset;
        if (!w.valid(n) || w.at(n).type == sand::pixel_type::none) {
            return true;
        }
    }
    return false;
}

auto find_boundary(const sand::world& w, int x, int y) -> glm::ivec2
{
    auto current = glm::ivec2{x, y};
    while (!is_boundary(w, current)) { current.y -= 1; }
    return current;
}

auto is_air_boundary(const sand::world& w, glm::ivec2 A, glm::ivec2 B) -> bool
{
    if (!w.valid(A) || !w.valid(B)) return true;
    return (w.at(A).type == sand::pixel_type::none || w.at(B).type == sand::pixel_type::none)
        && (w.at(A).type != w.at(B).type);
}

auto is_reachable_neighbour(const std::unordered_set<glm::ivec2>& points, const sand::world& w, glm::ivec2 src, glm::ivec2 dst) -> bool
{
    // ensure adjacent
    if (!points.contains(src) || !points.contains(dst)) {
        return false;
    }

    if (glm::abs(src.x - dst.x) + glm::abs(src.y - dst.y) != 1) {
        return false; // adjacent
    }

    if (src.x == dst.x) { // vertical
        if (dst.y == src.y - 1) { // dst on top
            return is_air_boundary(w, dst, dst + glm::ivec2{-1, 0});
        } else { // src on top
            return is_air_boundary(w, src, src + glm::ivec2{-1, 0});
        }
    } else { // horizonal
        if (dst.x == src.x - 1) { // dst to left
            return is_air_boundary(w, dst, dst + glm::ivec2{0, -1});
        } else { // src to left
            return is_air_boundary(w, src, src + glm::ivec2{0, -1});
        }
    }
}

auto get_boundary(const sand::world& w, int x, int y) -> std::vector<glm::ivec2>
{
    const auto points = flood_fill(w, x, y);

    auto ret = std::vector<glm::ivec2>{};
    auto current = find_boundary(w, x, y);
    ret.push_back(current);
    
    // Find second point
    bool found_second = false;
    for (const auto offset : offsets) {
        const auto neigh = current + offset;
        if (is_reachable_neighbour(points, w, current, neigh)) {
            current = neigh;
            ret.push_back(current);
            found_second = true;
            break;
        }
    }
    assert(found_second);

    // continue until we get back to the start
    while (current != ret.front()) {
        bool found = false;
        for (const auto offset : offsets) {
            const auto neigh = current + offset;
            if (is_reachable_neighbour(points, w, current, neigh) && neigh != ret.rbegin()[1]) {
                current = neigh;
                found = true;
                ret.push_back(current);
                break;
            }
        }
        if (!found) {
            return ret; // hmm
        }
    }

    ret.pop_back(); // last element equals the first, so remove it otherwise douglas is unhappy
    return ret;
}

float perpendicular_distance(const glm::ivec2& p, const glm::ivec2& a, const glm::ivec2& b) {
    // Vector AB
    glm::ivec2 ab = b - a;
    // Vector AP
    glm::ivec2 ap = p - a;
    
    // Cross product to get the area of the parallelogram formed by AB and AP
    float cross_product = ab.x * ap.y - ab.y * ap.x;
    // Length of the line segment AB
    float length_ab = std::sqrt(ab.x * ab.x + ab.y * ab.y);
    
    // Perpendicular distance is the absolute value of the cross product divided by the length of AB
    return std::abs(cross_product) / length_ab;
}

// Douglas-Peucker algorithm
auto douglas_peucker(
    const std::vector<glm::ivec2>& points, float tolerance) -> std::vector<glm::ivec2>
{
    if (points.size() < 2) {
        return points;
    }

    // Find the point with the maximum distance
    float max_dist = 0.0f;
    size_t index = 0;
    
    for (size_t i = 2; i < points.size() - 1; ++i) {
        float dist = perpendicular_distance(points[i], points.front(), points.back());
        if (dist > max_dist) {
            max_dist = dist;
            index = i;
        }
    }

    // If the maximum distance is greater than tolerance, split the curve
    if (max_dist > tolerance) {
        // Recursively apply to the two sub-segments
        std::vector<glm::ivec2> left(points.begin(), points.begin() + index + 1);
        std::vector<glm::ivec2> right(points.begin() + index, points.end());
        
        auto left_segment = douglas_peucker(left, tolerance);
        const auto right_segment = douglas_peucker(right, tolerance);

        // Merge the results
        left_segment.insert(left_segment.end(), right_segment.begin() + 1, right_segment.end());
        return left_segment;
    } else {
        // If the maximum distance is less than or equal to tolerance, just take the endpoints
        return { points.front(), points.back() };
    }
}

auto calc_boundary(const sand::world& w, int x, int y) -> std::vector<glm::ivec2>
{
    const auto points = get_boundary(w, x, y);
    const auto simplified = douglas_peucker(points, 1.5);
    return points;
}

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
    auto player          = sand::player_controller(physics, 5);
    auto shape_renderer  = sand::shape_renderer{};

    auto ground = std::vector<static_physics_box>{
        {physics, {128, 256 + 5}, 256, 10, {1.0, 1.0, 0.0, 1.0}},
    };

    auto file = std::ifstream{"save2.bin", std::ios::binary};
    auto archive = cereal::BinaryInputArchive{file};
    archive(*world);
    world->wake_all_chunks();

    auto points = calc_boundary(*world, 122, 233);
    auto count = 0;

    while (window.is_running()) {
        const double dt = timer.on_update();

        mouse.on_new_frame();
        keyboard.on_new_frame();
        
        window.poll_events();
        window.clear();

        accumulator += dt;
        bool updated = false;
        while (accumulator > sand::config::time_step) {
            accumulator -= sand::config::time_step;
            updated = true;

            sand::update(*world);
            player.update(keyboard);
            physics.Step(sand::config::time_step, 8, 3);
            count++;
            if (count % 5 == 0) {
                if (world->at({122, 233}).type == sand::pixel_type::rock) {
                    points = calc_boundary(*world, 122, 233);
                } else {
                    points = {};
                }
            }
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
        if (display_ui(editor, *world, physics, timer, window, camera, player)) {
            updated = true;
        }

        // Render and display the world
        world_renderer.bind();
        if (updated) {
            world_renderer.update(*world, editor.show_chunks, camera);
        }
        world_renderer.draw();

        shape_renderer.begin_frame(camera);

        shape_renderer.draw_circle(player.centre(), {1.0, 1.0, 0.0, 1.0}, player.radius());
        
        for (const auto& obj : ground) {
            shape_renderer.draw_quad(obj.centre(), obj.width(), obj.height(), obj.angle(), obj.colour());
        }
        
        // Testing the line renderer
        for (const auto& obj : ground) {
            const auto& centre = obj.centre();

            const auto cos = glm::cos(obj.angle());
            const auto sin = glm::sin(obj.angle());
            const auto rotation = glm::mat2{cos, sin, -sin, cos};

            const auto rW = rotation * glm::vec2{obj.width() / 2.0, 0.0};
            const auto rH = rotation * glm::vec2{0.0, obj.height() / 2.0};

            const auto tl = centre - rW - rH;
            const auto tr = centre + rW - rH;
            const auto bl = centre - rW + rH;
            const auto br = centre + rW + rH;

            shape_renderer.draw_line(tl, tr, {1, 0, 0, 1}, {0, 0, 1, 1}, 1);
            shape_renderer.draw_line(tr, br, {1, 0, 0, 1}, {0, 0, 1, 1}, 1);
            shape_renderer.draw_line(br, bl, {1, 0, 0, 1}, {0, 0, 1, 1}, 1);
            shape_renderer.draw_line(bl, tl, {1, 0, 0, 1}, {0, 0, 1, 1}, 1);
        }
        if (points.size() >= 2) {
            for (size_t i = 0; i != points.size() - 1; i++) {
                shape_renderer.draw_line({points[i]}, {points[i+1]}, {1,0,0,1}, {1,0,0,1}, 1);
                shape_renderer.draw_circle({points[i]}, {0, 0, 1, 1}, 0.25);
                
            }
        }
        //for (const auto point : points) {
        //    shape_renderer.draw_circle(point, {1, 1, 0, 1}, 0.25);
        //}
        shape_renderer.end_frame();
        
        // Display the UI
        ui.end_frame();

        window.swap_buffers();
    }
    
    return 0;
}