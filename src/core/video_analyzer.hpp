#pragma once

#include "frame_data.hpp"
#include <opencv2/opencv.hpp>
#include <functional>
#include <atomic>
#include <mutex>
#include <string>

namespace sharpctl {

class VideoAnalyzer {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;
    using SampleCallback = std::function<void(const FrameData& sample)>;
    // SearchCallback: windowStart, windowEnd, currentTime, bestTime, bestSharpness
    using SearchCallback = std::function<void(double, double, double, double, double)>;

    VideoAnalyzer() = default;
    ~VideoAnalyzer();

    // Open a video file
    bool openVideo(const std::string& path);
    void closeVideo();
    bool isOpen() const { return cap_.isOpened(); }

    // Get video information
    const VideoInfo& getVideoInfo() const { return videoInfo_; }

    // Calculate sharpness using specified algorithm
    static double calculateSharpness(const cv::Mat& frame, SharpnessAlgorithm algo = SharpnessAlgorithm::Laplacian);

    // Get a single frame at a specific time
    bool getFrameAt(double timeSec, cv::Mat& outFrame);

    // Analyze full video to get sharpness data for graph
    // This samples at regular intervals for the timeline visualization
    bool analyzeFullVideo(const AnalysisParams& params,
                          std::vector<FrameData>& outSamples,
                          ProgressCallback progressCb = nullptr,
                          SampleCallback sampleCb = nullptr);

    // Find optimal frames using the search window algorithm
    bool findOptimalFrames(const AnalysisParams& params,
                           const std::vector<FrameData>& allSamples,
                           std::vector<FrameData>& outSelected,
                           ProgressCallback progressCb = nullptr,
                           SearchCallback searchCb = nullptr);

    // Export selected frames to disk
    bool exportFrames(const std::vector<FrameData>& frames,
                      const std::string& outputDir,
                      ProgressCallback progressCb = nullptr);

    // Cancel ongoing operation
    void cancel() { cancelled_.store(true); }
    void resetCancel() { cancelled_.store(false); }
    bool isCancelled() const { return cancelled_.load(); }

private:
    cv::VideoCapture cap_;
    VideoInfo videoInfo_;
    std::atomic<bool> cancelled_{false};
    mutable std::recursive_mutex capMutex_;
};

}  // namespace sharpctl
