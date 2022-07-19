#include "pixel.h"
#include "utility.hpp"

#include <cstdlib>
#include <vector>

namespace sand {
namespace {

auto light_noise() -> glm::vec4
{
    return {
        random_from_range(-0.04f, 0.04f),
        random_from_range(-0.04f, 0.04f),
        random_from_range(-0.04f, 0.04f),
        1.0f
    };
}

}

auto pixel::properties() const -> const pixel_properties&
{
    switch (type) {
        case pixel_type::none: {
            static constexpr auto px = pixel_properties{
                .corrosion_resist = 1.0f
            };
            return px;
        }
        case pixel_type::sand: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::movable_solid,
                .inertial_resistance = 0.1f,
                .horizontal_transfer = 0.3f,
                .corrosion_resist = 0.3f
            };
            return px;
        }
        case pixel_type::dirt: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::movable_solid,
                .inertial_resistance = 0.4f,
                .horizontal_transfer = 0.2f,
                .corrosion_resist = 0.5f
            };
            return px;
        }
        case pixel_type::coal: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::movable_solid,
                .inertial_resistance = 0.95f,
                .horizontal_transfer = 0.1f,
                .corrosion_resist = 0.8f,
                .flammability = 0.02f
            };
            return px;
        }
        case pixel_type::water: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::liquid,
                .dispersion_rate = 5,
                .corrosion_resist = 1.0f,
            };
            return px;
        }
        case pixel_type::lava: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::liquid,
                .dispersion_rate = 1,
                .corrosion_resist = 1.0f,
                .affect_neighbour = [](pixel& me, pixel& them) {
                    if (them.type == pixel_type::water) {
                        them = pixel::steam();
                    }
                    if (random_from_range(0.0f, 1.0f) < them.properties().flammability) {
                        them.is_burning = true;
                    }
                }
            };
            return px;
        }
        case pixel_type::acid: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::liquid,
                .dispersion_rate = 1,
                .corrosion_resist = 1.0f,
                .affect_neighbour = [](pixel& me, pixel& them) {
                    const auto& props = them.properties();
                    if (random_from_range(0.0f, 1.0f) > props.corrosion_resist) {
                        them = pixel::air();
                        if (random_from_range(0.0f, 1.0f) > 0.9f) me = pixel::air();
                    }
                }
            };
            return px;
        }
        case pixel_type::rock: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::immovable_solid,
                .corrosion_resist = 0.95f
            };
            return px;
        }
        case pixel_type::titanium: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::immovable_solid,
                .corrosion_resist = 1.0f
            };
            return px;
        }
        case pixel_type::steam: {
            static constexpr auto px = pixel_properties{
                .movement = pixel_movement::gas,
                .dispersion_rate = 9,
                .corrosion_resist = 0.0f
            };
            return px;
        }
        default: {
            print("ERROR: Unknown pixel type {}\n", static_cast<int>(type));
            static constexpr auto px = pixel_properties{};
            return px;
        }
    }
}

auto pixel::air() -> pixel
{
    return pixel{
        .type = pixel_type::none,
        .colour = from_hex(0x2C3A47)
    };
}

auto pixel::sand() -> pixel
{
    return {
        .type = pixel_type::sand,
        .colour = from_hex(0xF8EFBA) + light_noise(),
        .is_falling = true
    };
}

auto pixel::coal() -> pixel
{
    return {
        .type = pixel_type::coal,
        .colour = from_hex(0x1E272E) + light_noise(),
        .is_falling = true
    };
}

auto pixel::dirt() -> pixel
{
    return {
        .type = pixel_type::dirt,
        .colour = from_hex(0x5C1D06) + light_noise(),
        .is_falling = true
    };
}

auto pixel::rock() -> pixel
{
    return {
        .type = pixel_type::rock,
        .colour = from_hex(0xC8C8C8) + light_noise()
    };
}

auto pixel::water() -> pixel
{
    return {
        .type = pixel_type::water,
        .colour = from_hex(0x1B9CFC) + light_noise()
    };
}

auto pixel::lava() -> pixel
{
    return {
        .type = pixel_type::lava,
        .colour = from_hex(0xF97F51) + light_noise()
    };
}

auto pixel::acid() -> pixel
{
    return {
        .type = pixel_type::acid,
        .colour = from_hex(0x2ed573) + light_noise()
    };
}

auto pixel::steam() -> pixel
{
    return {
        .type = pixel_type::steam,
        .colour = from_hex(0x9AECDB) + light_noise()
    };
}

auto pixel::titanium() -> pixel
{
    return {
        .type = pixel_type::titanium,
        .colour = from_hex(0xDFE4EA)
    };
}


}