# CMake-Glfw-OpenGL-Template

## What is this?

CMake based C++ Project template with support for glfw, glad, imgui, spdlog, tracy, stbi, cgltf

This template is from https://github.com/deccer/CMake-Glfw-OpenGL-Template
![Screenshot](screenshots/screenshot.png)

## For whom is this for?

- Prototyping
- Game Jams
- Getting started with programming
- Getting started with graphics programming

## What you need:

- `CMake`
- for VSCode the `CMake Tools` extension from Microsoft
- a compiler of your choice (Clang, GCC, MSVC)

## Assumptions

This project template assumes the following things

- OpenGL 4.6 support (can be changed in `Application.cpp:59-60`)
- Resolution >= 1920x1080 so that you can actually use the window (can be changed in `Application.cpp:66-67`)
