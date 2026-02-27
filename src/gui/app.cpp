#include "app.hpp"
#include "panels/control_panel.hpp"
#include "panels/timeline_panel.hpp"
#include "panels/preview_panel.hpp"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include <GL/gl.h>
#include <iostream>
#include <filesystem>
#include <fstream>

namespace sharpctl {

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << std::endl;
        return false;
    }

    // GL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window
    window_ = SDL_CreateWindow(
        "sharpctl - Video Frame Extractor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth_, windowHeight_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window_) {
        std::cerr << "SDL_CreateWindow error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set window icon
    setWindowIcon();

    // Create GL context
    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        std::cerr << "SDL_GL_CreateContext error: " << SDL_GetError() << std::endl;
        return false;
    }
    SDL_GL_MakeCurrent(window_, glContext_);
    SDL_GL_SetSwapInterval(1);  // VSync

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    setupImGuiStyle();

    // Setup backends
    ImGui_ImplSDL2_InitForOpenGL(window_, glContext_);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void App::setWindowIcon() {
    // Try to load icon from common locations
    std::vector<std::string> iconPaths = {
        "sharpctl_logo.png",
        "../sharpctl_logo.png",
        "/usr/share/sharpctl/sharpctl_logo.png"
    };

    cv::Mat icon;
    for (const auto& path : iconPaths) {
        icon = cv::imread(path, cv::IMREAD_UNCHANGED);
        if (!icon.empty()) break;
    }

    if (icon.empty()) return;

    // Convert to RGBA if needed
    cv::Mat rgba;
    if (icon.channels() == 4) {
        cv::cvtColor(icon, rgba, cv::COLOR_BGRA2RGBA);
    } else if (icon.channels() == 3) {
        cv::cvtColor(icon, rgba, cv::COLOR_BGR2RGBA);
    } else {
        return;
    }

    // Create SDL surface
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        rgba.data,
        rgba.cols, rgba.rows,
        32,
        rgba.step,
        0x000000FF,  // R mask
        0x0000FF00,  // G mask
        0x00FF0000,  // B mask
        0xFF000000   // A mask
    );

    if (surface) {
        SDL_SetWindowIcon(window_, surface);
        SDL_FreeSurface(surface);
    }
}

void App::setupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Modern dark theme with accent colors
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);

    ImVec4* colors = style.Colors;

    // Background colors
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.95f);

    // Border
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frame backgrounds
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);

    // Title bar
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.75f);

    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.43f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.50f, 0.53f, 1.00f);

    // Accent color (teal/cyan)
    const ImVec4 accent = ImVec4(0.26f, 0.75f, 0.75f, 1.00f);
    const ImVec4 accentHover = ImVec4(0.30f, 0.85f, 0.85f, 1.00f);
    const ImVec4 accentActive = ImVec4(0.22f, 0.65f, 0.65f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = accent;
    colors[ImGuiCol_ButtonActive] = accentActive;

    // Headers
    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = accent;
    colors[ImGuiCol_HeaderActive] = accentActive;

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered] = accent;
    colors[ImGuiCol_TabActive] = accentActive;
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);

    // Slider/Checkbox
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentHover;
    colors[ImGuiCol_CheckMark] = accent;

    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    // Separator
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accentActive;

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.52f, 1.00f);

    // Plot colors
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = accentHover;
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHover;

    // Table
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.15f, 0.15f, 0.18f, 0.50f);

    // Docking
    colors[ImGuiCol_DockingPreview] = accent;
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);

    // Modal dimming
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
}

void App::run() {
    bool running = true;

    while (running) {
        handleEvents(running);
        renderFrame();
    }
}

void App::handleEvents(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT) {
            running = false;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(window_)) {
            running = false;
        }

        // Handle drag-drop
        if (event.type == SDL_DROPFILE) {
            char* droppedFile = event.drop.file;
            loadVideo(droppedFile);
            SDL_free(droppedFile);
        }

        // Keyboard shortcuts
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE && isAnalyzing()) {
                cancelAnalysis();
            }
        }
    }

    // Update window size
    SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);
}

void App::renderFrame() {
    updatePerfStats();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Create dockspace over full window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking |
                                    ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                                    ImGuiWindowFlags_NoNavFocus |
                                    ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();

    // Render panels
    gui::renderControlPanel(*this);
    gui::renderPreviewPanel(*this);
    gui::renderTimelinePanel(*this);

    // Rendering
    ImGui::Render();
    glViewport(0, 0, windowWidth_, windowHeight_);
    glClearColor(0.10f, 0.10f, 0.12f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window_);
}

void App::shutdown() {
    // Wait for analysis thread
    if (analysisThread_.joinable()) {
        cancelAnalysis();
        analysisThread_.join();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (glContext_) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void App::loadVideo(const std::string& path) {
    if (isAnalyzing()) {
        cancelAnalysis();
        if (analysisThread_.joinable()) {
            analysisThread_.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        allSamples_.clear();
    }
    selectedFrames_.clear();
    progress_.store(0.0f);

    if (analyzer_.openVideo(path)) {
        videoInfo_ = analyzer_.getVideoInfo();
        configDirty_ = false;

        // Try to load existing config
        if (loadConfig()) {
            std::lock_guard<std::mutex> lock(statusMutex_);
            statusText_ = "Video loaded with config: " + videoInfo_.path;
        } else {
            std::lock_guard<std::mutex> lock(statusMutex_);
            statusText_ = "Video loaded: " + videoInfo_.path;
        }
    } else {
        videoInfo_ = VideoInfo{};
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            statusText_ = "Failed to load video";
        }
    }
}

void App::startAnalysis() {
    if (isAnalyzing() || !analyzer_.isOpen()) return;

    if (analysisThread_.joinable()) {
        analysisThread_.join();
    }

    // Clear samples before starting
    {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        allSamples_.clear();
    }
    selectedFrames_.clear();

    analyzing_.store(true);
    progress_.store(0.0f);
    analysisStartTime_ = std::chrono::steady_clock::now();
    analyzer_.resetCancel();

    analysisThread_ = std::thread([this]() {
        // First pass: analyze full video for graph
        std::vector<FrameData> samples;
        bool success = analyzer_.analyzeFullVideo(params_, samples,
            [this](float progress, const std::string& status) {
                progress_.store(progress * 0.5f);  // 0-50%
                std::lock_guard<std::mutex> lock(statusMutex_);
                statusText_ = status;
            },
            [this](const FrameData& sample) {
                // Update allSamples_ in real-time for live graph updates
                std::lock_guard<std::mutex> lock(samplesMutex_);
                allSamples_.push_back(sample);
            });

        if (success && !analyzer_.isCancelled()) {
            // Second pass: find optimal frames
            std::vector<FrameData> localSamples;
            {
                std::lock_guard<std::mutex> lock(samplesMutex_);
                localSamples = allSamples_;
            }
            std::vector<FrameData> selected;
            success = analyzer_.findOptimalFrames(params_, localSamples, selected,
                [this](float progress, const std::string& status) {
                    progress_.store(0.5f + progress * 0.5f);  // 50-100%
                    std::lock_guard<std::mutex> lock(statusMutex_);
                    statusText_ = status;
                },
                [this](double winStart, double winEnd, double currentT, double bestT, double bestSharp) {
                    SearchState state;
                    state.active = (winStart != 0 || winEnd != 0);
                    state.windowStart = winStart;
                    state.windowEnd = winEnd;
                    state.currentSearchTime = currentT;
                    state.bestTime = bestT;
                    state.bestSharpness = bestSharp;
                    setSearchState(state);
                });

            if (success && !analyzer_.isCancelled()) {
                selectedFrames_ = std::move(selected);
                configDirty_ = true;
            }
        }

        // Clear search state
        setSearchState(SearchState{});

        analyzing_.store(false);
        progress_.store(1.0f);
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            statusText_ = analyzer_.isCancelled() ? "Analysis cancelled" : "Analysis complete";
        }
    });
}

void App::cancelAnalysis() {
    analyzer_.cancel();
}

void App::exportFrames(const std::string& outputDir) {
    if (isAnalyzing() || selectedFrames_.empty()) return;

    if (analysisThread_.joinable()) {
        analysisThread_.join();
    }

    analyzing_.store(true);
    progress_.store(0.0f);
    analysisStartTime_ = std::chrono::steady_clock::now();
    analyzer_.resetCancel();

    analysisThread_ = std::thread([this, outputDir]() {
        analyzer_.exportFrames(selectedFrames_, outputDir,
            [this](float progress, const std::string& status) {
                progress_.store(progress);
                std::lock_guard<std::mutex> lock(statusMutex_);
                statusText_ = status;
            });

        analyzing_.store(false);
        progress_.store(1.0f);
        {
            std::lock_guard<std::mutex> lock(statusMutex_);
            statusText_ = analyzer_.isCancelled() ? "Export cancelled" : "Export complete";
        }
    });
}

std::string App::getStatusText() {
    std::lock_guard<std::mutex> lock(statusMutex_);
    return statusText_;
}

float App::getRemainingSeconds() const {
    if (!analyzing_.load()) return 0.0f;

    float progress = progress_.load();
    if (progress <= 0.01f) return -1.0f;  // Not enough data to estimate

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - analysisStartTime_).count();

    float totalEstimate = elapsed / progress;
    float remaining = totalEstimate - elapsed;

    return std::max(0.0f, remaining);
}

void App::toggleFrameSelection(int index) {
    if (index >= 0 && index < static_cast<int>(selectedFrames_.size())) {
        selectedFrames_[index].selected = !selectedFrames_[index].selected;
        configDirty_ = true;
    }
}

void App::addFrameAtTime(double time) {
    if (!analyzer_.isOpen()) return;

    cv::Mat frame;
    if (analyzer_.getFrameAt(time, frame)) {
        FrameData data;
        data.time = time;
        data.sharpness = VideoAnalyzer::calculateSharpness(frame, params_.algorithm);
        data.selected = true;

        // Create thumbnail
        const int thumbHeight = 120;
        const int thumbWidth = static_cast<int>(thumbHeight * frame.cols / frame.rows);
        cv::resize(frame, data.thumbnail, cv::Size(thumbWidth, thumbHeight));

        // Insert in sorted order
        auto it = std::lower_bound(selectedFrames_.begin(), selectedFrames_.end(), data,
            [](const FrameData& a, const FrameData& b) { return a.time < b.time; });
        selectedFrames_.insert(it, data);
        configDirty_ = true;
    }
}

int App::getSelectedCount() const {
    int count = 0;
    for (const auto& f : selectedFrames_) {
        if (f.selected) count++;
    }
    return count;
}

bool App::saveConfig() {
    if (videoInfo_.path.empty()) return false;

    std::string configPath = getConfigPath(videoInfo_.path);
    cv::FileStorage fs(configPath, cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;

    fs << "version" << 1;

    fs << "params" << "{";
    fs << "interval_sec" << params_.intervalSec;
    fs << "search_window_sec" << params_.searchWindowSec;
    fs << "search_step_sec" << params_.searchStepSec;
    fs << "sample_step_sec" << params_.sampleStepSec;
    fs << "algorithm" << (params_.algorithm == SharpnessAlgorithm::FFT ? "FFT" : "Laplacian");
    fs << "}";

    fs << "samples" << "[";
    {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        for (const auto& sample : allSamples_) {
            fs << "{" << "time" << sample.time << "sharpness" << sample.sharpness << "}";
        }
    }
    fs << "]";

    fs << "selected_frames" << "[";
    for (const auto& frame : selectedFrames_) {
        if (frame.selected) {
            fs << "{" << "time" << frame.time << "sharpness" << frame.sharpness << "}";
        }
    }
    fs << "]";

    fs.release();
    configDirty_ = false;

    {
        std::lock_guard<std::mutex> lock(statusMutex_);
        statusText_ = "Config saved: " + configPath;
    }

    return true;
}

bool App::loadConfig() {
    if (videoInfo_.path.empty()) return false;

    std::string configPath = getConfigPath(videoInfo_.path);
    if (!std::filesystem::exists(configPath)) return false;

    cv::FileStorage fs(configPath, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;

    // Check version
    int version = 0;
    fs["version"] >> version;
    if (version < 1) {
        fs.release();
        return false;
    }

    // Read params
    cv::FileNode paramsNode = fs["params"];
    if (!paramsNode.empty()) {
        params_.intervalSec = static_cast<float>(paramsNode["interval_sec"]);
        params_.searchWindowSec = static_cast<float>(paramsNode["search_window_sec"]);
        params_.searchStepSec = static_cast<float>(paramsNode["search_step_sec"]);
        params_.sampleStepSec = static_cast<float>(paramsNode["sample_step_sec"]);

        std::string algoStr;
        paramsNode["algorithm"] >> algoStr;
        params_.algorithm = (algoStr == "FFT") ? SharpnessAlgorithm::FFT : SharpnessAlgorithm::Laplacian;
    }

    // Read samples (graph data)
    cv::FileNode samplesNode = fs["samples"];
    if (!samplesNode.empty()) {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        allSamples_.clear();
        for (const auto& sn : samplesNode) {
            FrameData fd;
            fd.time = static_cast<double>(sn["time"]);
            fd.sharpness = static_cast<double>(sn["sharpness"]);
            fd.selected = false;
            allSamples_.push_back(fd);
        }
    }

    // Read selected frames
    selectedFrames_.clear();
    cv::FileNode framesNode = fs["selected_frames"];
    for (const auto& fn : framesNode) {
        FrameData fd;
        fd.time = static_cast<double>(fn["time"]);
        fd.sharpness = static_cast<double>(fn["sharpness"]);
        fd.selected = true;

        // Load thumbnail for this frame
        cv::Mat frame;
        if (analyzer_.getFrameAt(fd.time, frame)) {
            const int thumbHeight = 120;
            const int thumbWidth = static_cast<int>(thumbHeight * frame.cols / frame.rows);
            cv::resize(frame, fd.thumbnail, cv::Size(thumbWidth, thumbHeight));
        }

        selectedFrames_.push_back(fd);
    }

    fs.release();
    configDirty_ = false;

    return true;
}

void App::updatePerfStats() {
    // Read CPU usage from /proc/stat
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string cpu;
        long long user, nice, system, idle, iowait, irq, softirq, steal;
        statFile >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        statFile.close();

        long long idleTime = idle + iowait;
        long long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;

        if (perfStats_.prevTotalTime > 0) {
            long long totalDelta = totalTime - perfStats_.prevTotalTime;
            long long idleDelta = idleTime - perfStats_.prevIdleTime;
            if (totalDelta > 0) {
                perfStats_.currentCpu = 100.0f * (1.0f - static_cast<float>(idleDelta) / totalDelta);
            }
        }

        perfStats_.prevIdleTime = idleTime;
        perfStats_.prevTotalTime = totalTime;
    }

    // Try to read GPU usage (AMD via sysfs)
    std::ifstream gpuFile("/sys/class/drm/card0/device/gpu_busy_percent");
    if (gpuFile.is_open()) {
        int gpuPercent = 0;
        gpuFile >> gpuPercent;
        gpuFile.close();
        perfStats_.currentGpu = static_cast<float>(gpuPercent);
    } else {
        // Try card1 (some systems have card0 as integrated)
        gpuFile.open("/sys/class/drm/card1/device/gpu_busy_percent");
        if (gpuFile.is_open()) {
            int gpuPercent = 0;
            gpuFile >> gpuPercent;
            gpuFile.close();
            perfStats_.currentGpu = static_cast<float>(gpuPercent);
        } else {
            // Try NVIDIA (nvidia-smi leaves a file or we could use NVML)
            // For simplicity, show 0 if no GPU stats available
            perfStats_.currentGpu = 0.0f;
        }
    }

    // Update history ring buffer
    perfStats_.cpuHistory[perfStats_.historyIndex] = perfStats_.currentCpu;
    perfStats_.gpuHistory[perfStats_.historyIndex] = perfStats_.currentGpu;
    perfStats_.historyIndex = (perfStats_.historyIndex + 1) % PerfStats::HISTORY_SIZE;
}

}  // namespace sharpctl
