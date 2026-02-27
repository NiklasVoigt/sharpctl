cmake_minimum_required(VERSION 3.16)

project(sharpctl
    VERSION 1.0
    DESCRIPTION "Video frame extraction with sharpness optimization"
    LANGUAGES CXX
)

# Use modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(SHARPCTL_BUILD_GUI "Build the GUI version (requires SDL2, OpenGL)" ON)

# Find OpenCV
find_package(OpenCV REQUIRED)

# Find OpenMP
find_package(OpenMP REQUIRED)

# Core library (shared between CLI and GUI)
add_library(sharpctl_core STATIC
    src/core/video_analyzer.cpp
)
target_include_directories(sharpctl_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(sharpctl_core
    PUBLIC ${OpenCV_LIBS}
    PUBLIC OpenMP::OpenMP_CXX
)

if(SHARPCTL_BUILD_GUI)
    # Find SDL2 and OpenGL
    find_package(SDL2 REQUIRED)
    find_package(OpenGL REQUIRED)

    # ImGui library
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC
        SDL2::SDL2
        OpenGL::GL
    )

    # ImPlot library
    set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/implot)
    add_library(implot STATIC
        ${IMPLOT_DIR}/implot.cpp
        ${IMPLOT_DIR}/implot_items.cpp
    )
    target_include_directories(implot PUBLIC
        ${IMPLOT_DIR}
    )
    target_link_libraries(implot PUBLIC
        imgui
    )

    # portable-file-dialogs (header-only)
    add_library(pfd INTERFACE)
    target_include_directories(pfd INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/portable-file-dialogs
    )

    # GUI executable
    add_executable(sharpctl
        src/main.cpp
        src/gui/app.cpp
        src/gui/panels/control_panel.cpp
        src/gui/panels/timeline_panel.cpp
        src/gui/panels/preview_panel.cpp
        src/gui/widgets/frame_texture.cpp
    )
    target_link_libraries(sharpctl
        PRIVATE
        sharpctl_core
        imgui
        implot
        pfd
        SDL2::SDL2
        OpenGL::GL
    )
    target_compile_definitions(sharpctl PRIVATE SHARPCTL_GUI_ENABLED)
else()
    # CLI-only executable
    add_executable(sharpctl
        src/main.cpp
    )
    target_link_libraries(sharpctl
        PRIVATE sharpctl_core
    )
endif()

# Show dependency info
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
if(SHARPCTL_BUILD_GUI)
    message(STATUS "Building with GUI support")
    message(STATUS "SDL2 found: ${SDL2_FOUND}")
else()
    message(STATUS "Building CLI-only version")
endif()
