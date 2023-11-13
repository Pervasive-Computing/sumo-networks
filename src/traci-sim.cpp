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

    Camera2D camera = { 0 };
    camera.target = circleCenter;
    camera.offset = { screen_width / 2.0f, screen_height / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    SetTargetFPS(60); // Set target frames per second

    // MaximizeWindow();
    // Main game loop
    Vector2 prev_mouse_pos = GetMousePosition();
    
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

        // move camera with arrow keys
        // or the wasd keys if you're a cool kid
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            camera.target.x -= 10.0f / camera.zoom;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            camera.target.x += 10.0f / camera.zoom;
        }
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
            camera.target.y -= 10.0f / camera.zoom;
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            camera.target.y += 10.0f / camera.zoom;
        }
        // drag when either lmb or middle mouse is down
        // if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        //     camera.target.x -= (GetMouseX() - GetMouseX()) / camera.zoom;
        //     camera.target.y -= (GetMouseY() - GetMouseY()) / camera.zoom;
        // }
        // the above version doesnt work as GetMouseX() - GetMouseX() yields 0
        // so we have to use the below version
        Vector2 mouse_pos = GetMousePosition();
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            camera.target.x -= (mouse_pos.x - prev_mouse_pos.x) / camera.zoom;
            camera.target.y -= (mouse_pos.y - prev_mouse_pos.y) / camera.zoom;
        }
        prev_mouse_pos = mouse_pos;

        // zoom camera with mouse wheel
        camera.zoom += ((float)GetMouseWheelMove() * 0.05f * camera.zoom);
        // clamp camera zoom between 0.1 and 3.0
        if (camera.zoom > 3.0f) camera.zoom = 3.0f;
        else if (camera.zoom < 0.1f) camera.zoom = 0.1f;

        // Update
        angle += 0.02f; // Increase the angle for rotation
        
        // Calculate dot position
        float dotX = circleCenter.x + cos(angle) * circleRadius;
        float dotY = circleCenter.y + sin(angle) * circleRadius;


        // std::cerr << mouse_pos.x << " " << mouse_pos.y << std::endl;

        const f32 rect_width = 30;
        const f32 rect_height = rect_width;

        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode2D(camera);
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

            EndMode2D();

            DrawRectangle(mouse_pos.x - rect_width / 2, mouse_pos.y - rect_height / 2, rect_width, rect_height, BLUE);
            // write the camera zoom at the top right corner
            DrawText(TextFormat("Camera Zoom: %.2f", camera.zoom), screen_width - 180, 20, 10, GRAY);
            DrawFPS(0, 0);
        EndDrawing();
    }

    // De-Initialization
    CloseWindow(); // Close window and OpenGL context


  return 0;
}
