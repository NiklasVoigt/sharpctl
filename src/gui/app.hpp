#pragma once

#include "core/video_analyzer.hpp"
#include <SDL.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <array>
#include <chrono>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace sharpctl {

// State for visualizing the search process
struct SearchState {
    bool active = false;
    double windowStart = 0.0;
    double windowEnd = 0.0;
    double currentSearchTime = 0.0;
    double bestTime = 0.0;
    double bestSharpness = 0.0;
};

// Performance monitoring data
struct PerfStats {
    static constexpr size_t HISTORY_SIZE = 120;  // ~2 seconds at 60fps

    std::array<float, HISTORY_SIZE> cpuHistory{};
    std::array<float, HISTORY_SIZE> gpuHistory{};
    size_t historyIndex = 0;

    float currentCpu = 0.0f;
    float currentGpu = 0.0f;

    // For CPU calculation
    long long prevIdleTime = 0;
    long long prevTotalTime = 0;
};

class App {
public:
    App();
    ~App();

    bool init();
    void run();
    void shutdown();

    // Access to shared state (for panels)
    VideoAnalyzer& getAnalyzer() { return analyzer_; }
    VideoInfo& getVideoInfo() { return videoInfo_; }
    AnalysisParams& getParams() { return params_; }
    std::vector<FrameData> getAllSamples() {
        std::lock_guard<std::mutex> lock(samplesMutex_);
        return allSamples_;
    }
    std::vector<FrameData>& getSelectedFrames() { return selectedFrames_; }

    // Progress tracking
    float getProgress() const { return progress_.load(); }
    std::string getStatusText();
    bool isAnalyzing() const { return analyzing_.load(); }
    float getRemainingSeconds() const;

    // Actions
    void loadVideo(const std::string& path);
    void startAnalysis();
    void cancelAnalysis();
    void exportFrames(const std::string& outputDir);

    // Config save/load
    bool saveConfig();
    bool loadConfig();
    bool hasUnsavedChanges() const { return configDirty_; }
    void markConfigDirty() { configDirty_ = true; }

    // Preview state
    void setHoveredTime(double time) { hoveredTime_ = time; }
    double getHoveredTime() const { return hoveredTime_; }
    void setPreviewFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(previewMutex_);
        previewFrame_ = frame.clone();
        previewDirty_ = true;
    }
    bool getPreviewFrame(cv::Mat& out) {
        std::lock_guard<std::mutex> lock(previewMutex_);
        if (previewDirty_) {
            out = previewFrame_.clone();
            previewDirty_ = false;
            return true;
        }
        return false;
    }

    // Selection management
    void toggleFrameSelection(int index);
    void addFrameAtTime(double time);
    int getSelectedCount() const;

    // Search visualization
    SearchState getSearchState() {
        std::lock_guard<std::mutex> lock(searchMutex_);
        return searchState_;
    }
    void setSearchState(const SearchState& state) {
        std::lock_guard<std::mutex> lock(searchMutex_);
        searchState_ = state;
    }

    // Performance stats
    void updatePerfStats();
    const PerfStats& getPerfStats() const { return perfStats_; }

private:
    void setupImGuiStyle();
    void setWindowIcon();
    void renderFrame();
    void handleEvents(bool& running);

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;

    VideoAnalyzer analyzer_;
    VideoInfo videoInfo_;
    AnalysisParams params_;
    std::vector<FrameData> allSamples_;
    std::vector<FrameData> selectedFrames_;

    // Threading
    std::thread analysisThread_;
    std::atomic<bool> analyzing_{false};
    std::atomic<float> progress_{0.0f};
    std::string statusText_;
    std::mutex statusMutex_;
    std::mutex samplesMutex_;
    std::chrono::steady_clock::time_point analysisStartTime_;

    // Preview
    double hoveredTime_ = -1.0;
    cv::Mat previewFrame_;
    bool previewDirty_ = false;
    std::mutex previewMutex_;

    // Search visualization
    SearchState searchState_;
    std::mutex searchMutex_;

    // Window state
    int windowWidth_ = 1400;
    int windowHeight_ = 900;

    // Config state
    bool configDirty_ = false;

    // Performance monitoring
    PerfStats perfStats_;
};

}  // namespace sharpctl
