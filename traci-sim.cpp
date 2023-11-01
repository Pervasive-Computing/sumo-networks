#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <cmath>
#include <iostream>

#include "raylib.h"
#include "cxxopts.hpp"

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

template <typename T> using Vec = std::vector<T>;
using String = std::string;

template <typename T> using Option = std::optional<T>;

auto main(int argc, char **argv) -> int {

    cxxopts::Options options(__FILE__, "Control sumo traci simulation");
    options.add_options()
        ("w,width", "window width", cxxopts::value<int>())
        ("h,height", "window height", cxxopts::value<int>())
        ("gui", "use `sumo-gui` instead of `sumo` to run the simulation", cxxopts::value<bool>())
        ("help", "show help information", cxxopts::value<bool>())
        ;

            auto result = options.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    const int monitor_width = GetMonitorWidth(0);
    const int monitor_height = GetMonitorHeight(0);
    
    const int screen_width = result.count("width") ? result["width"].as<int>() : 1920;
    const int screen_height = result.count("height") ? result["height"].as<int>() : 1080;
// Initialization
    // const int screenWidth = 1920;
    // const int screenHeight = 1080;

    InitWindow(screen_width, screen_height, "Raylib - Rotating Dot Around a Circle");

    Vector2 circleCenter = { static_cast<float>(screen_width / 2), static_cast<float>(screen_height / 2) };
    float circleRadius = 100.0f;
    float angle = 0.0f; // Angle of rotation

    SetTargetFPS(60); // Set target frames per second

    // MaximizeWindow();
    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Check if space is pressed
        if (IsKeyPressed(KEY_SPACE)) {
            std::cout << "space pressed" << std::endl;
            // Code to execute when space is pressed
            if (IsWindowMaximized()) {
                RestoreWindow();
            } else {
                MaximizeWindow();
            }
        }


        // Update
        angle += 0.02f; // Increase the angle for rotation
        
        // Calculate dot position
        float dotX = circleCenter.x + cos(angle) * circleRadius;
        float dotY = circleCenter.y + sin(angle) * circleRadius;


        Vector2 mouse_pos = GetMousePosition();
        std::cerr << mouse_pos.x << " " << mouse_pos.y << std::endl;

        const f32 rect_width = 30;
        const f32 rect_height = rect_width;

        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawCircleV(circleCenter, circleRadius, LIGHTGRAY); // Draw the circle
            // DrawCircle(dotX, dotY, 20.0f, RED); // Draw the rotating dot

            float offsets[] = { 0.0f, PI / 3.0f, -PI / 3.0f };
            for (int i = 0; i < 3; i++) {
                float offset = offsets[i];
                float dotx = circleCenter.x + cos(angle + offset) * circleRadius;
                float doty = circleCenter.y + sin(angle + offset) * circleRadius;
                DrawCircle(dotx,    doty, 20.0f, ORANGE);
                // DrawRectangle(dotx - rect_width / 2, doty - rect_height / 2, rect_width, rect_height, RED);
            }

            DrawRectangle(mouse_pos.x - rect_width / 2, mouse_pos.y - rect_height / 2, rect_width, rect_height, BLUE);
            DrawFPS(0,0);
        EndDrawing();
    }

    // De-Initialization
    CloseWindow(); // Close window and OpenGL context


  return 0;
}
