#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace sharpctl {

enum class SharpnessAlgorithm {
    FFT,         // High-frequency content (default)
    Laplacian    // Laplacian variance
};

struct FrameData {
    double time = 0.0;
    double sharpness = 0.0;
    bool selected = false;
    cv::Mat thumbnail;
};

struct VideoInfo {
    std::string path;
    double duration = 0.0;
    double fps = 0.0;
    int frameCount = 0;
    int width = 0;
    int height = 0;

    bool isValid() const { return fps > 0.0 && frameCount > 0; }
};

struct AnalysisParams {
    float intervalSec = 3.0f;
    float searchWindowSec = 0.5f;
    float searchStepSec = 0.02f;
    float sampleStepSec = 0.1f;  // For full video analysis (graph data)
    SharpnessAlgorithm algorithm = SharpnessAlgorithm::FFT;
};

struct AnalysisResult {
    std::vector<FrameData> allSamples;      // Full video sharpness data for graph
    std::vector<FrameData> selectedFrames;  // Optimal frames selected by algorithm
    bool cancelled = false;
    std::string error;
};

// Helper to get config file path for a video
inline std::string getConfigPath(const std::string& videoPath) {
    return videoPath + ".sharpctl";
}

}  // namespace sharpctl
