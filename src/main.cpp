#include <opencv2/opencv.hpp>
#include <opencv2/plot.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#include "core/video_analyzer.hpp"

#ifdef SHARPCTL_GUI_ENABLED
#include "gui/app.hpp"
#endif

namespace fs = std::filesystem;

namespace {

sharpctl::SharpnessAlgorithm parseAlgorithm(const std::string& name) {
    if (name == "laplacian") return sharpctl::SharpnessAlgorithm::Laplacian;
    return sharpctl::SharpnessAlgorithm::FFT;
}

}  // anonymous namespace

// CLI mode implementation
int runCli(int argc, char** argv) {
    // Parse flags
    bool showPlot = false;
    sharpctl::SharpnessAlgorithm algorithm = sharpctl::SharpnessAlgorithm::FFT;
    std::vector<char*> args;

    for (int i = 0; i < argc; ++i) {
        if (std::strcmp(argv[i], "--plot") == 0) {
            showPlot = true;
        } else if (std::strncmp(argv[i], "--algorithm=", 12) == 0) {
            algorithm = parseAlgorithm(argv[i] + 12);
        } else if (std::strcmp(argv[i], "--cli") != 0) {
            args.push_back(argv[i]);
        }
    }

    if (args.size() < 4) {
        std::cerr
            << "Usage:\n  " << args[0]
            << " <video_file> <output_folder> <target_interval_sec>"
               " [search_window_sec=0.5] [search_step_sec=0.02] [--plot] [--algorithm=<name>]\n\n"
            << "Algorithms:\n"
            << "  fft       - FFT-based (default, slower, higher quality)\n"
            << "  laplacian - Laplacian variance (faster, lower quality)\n\n"
            << "Example:\n  " << args[0] << " input.mp4 out 3 0.5 0.01 --plot --algorithm=fft\n";
        return 1;
    }

    const std::string videoPath = args[1];
    const std::string outDir = args[2];
    const double targetIntervalSec = std::stod(args[3]);
    const double searchWindowSec = (args.size() >= 5) ? std::stod(args[4]) : 0.5;
    const double searchStepSec = (args.size() >= 6) ? std::stod(args[5]) : 0.02;

    if (targetIntervalSec <= 0.0) {
        std::cerr << "Error: target_interval_sec must be > 0\n";
        return 1;
    }
    if (searchWindowSec < 0.0 || searchStepSec <= 0.0) {
        std::cerr << "Error: search_window_sec must be >= 0 and search_step_sec must be > 0\n";
        return 1;
    }

    fs::create_directories(outDir);

    sharpctl::VideoAnalyzer analyzer;
    if (!analyzer.openVideo(videoPath)) {
        std::cerr << "Error: Could not open video file\n";
        return 1;
    }

    const auto& videoInfo = analyzer.getVideoInfo();
    const double durationSec = videoInfo.duration;

    if (durationSec <= 0.0) {
        std::cerr << "Warning: duration unknown (FPS/frame count not reliable). Will sample until seeks fail.\n";
    }

    sharpctl::AnalysisParams params;
    params.intervalSec = static_cast<float>(targetIntervalSec);
    params.searchWindowSec = searchWindowSec;
    params.searchStepSec = searchStepSec;
    params.algorithm = algorithm;

    std::vector<sharpctl::FrameData> allSamples;
    std::vector<sharpctl::FrameData> selectedFrames;

    // Find optimal frames
    analyzer.findOptimalFrames(params, allSamples, selectedFrames,
        [](float progress, const std::string& status) {
            // Progress callback (could add progress bar here)
        });

    // Export frames
    int outIndex = 0;
    for (const auto& frameData : selectedFrames) {
        cv::Mat frame;
        if (analyzer.getFrameAt(frameData.time, frame)) {
            char filename[512];
            std::snprintf(filename, sizeof(filename),
                          "frame_%04d_t%.3f_var%.2f.jpg",
                          outIndex, frameData.time, frameData.sharpness);

            const fs::path outPath = fs::path(outDir) / filename;

            if (!cv::imwrite(outPath.string(), frame)) {
                std::cerr << "Error: failed writing " << outPath.string() << "\n";
                return 1;
            }

            std::cout << "Target t=" << (outIndex * targetIntervalSec)
                      << "s -> chosen t=" << frameData.time
                      << "s  var=" << frameData.sharpness
                      << "  saved: " << outPath.string() << "\n";
            outIndex++;
        }
    }

    // OPTIONAL: plot chosen sharpness values
    if (showPlot && !selectedFrames.empty()) {
        std::vector<double> chosenSharpness;
        for (const auto& f : selectedFrames) {
            chosenSharpness.push_back(f.sharpness);
        }

        cv::Mat yMat(chosenSharpness);
        yMat = yMat.reshape(1, static_cast<int>(chosenSharpness.size()));

        auto plot = cv::plot::Plot2d::create(yMat);
        plot->setPlotSize(1000, 600);

        cv::Mat plotImage;
        plot->render(plotImage);

        cv::namedWindow("Chosen frame sharpness", cv::WINDOW_NORMAL);
        cv::resizeWindow("Chosen frame sharpness", 1000, 600);
        cv::imshow("Chosen frame sharpness", plotImage);
        cv::waitKey(0);
    }

    return 0;
}

#ifdef SHARPCTL_GUI_ENABLED
// GUI mode implementation
int runGui() {
    sharpctl::App app;

    if (!app.init()) {
        std::cerr << "Failed to initialize GUI\n";
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
#endif

int main(int argc, char** argv) {
    // Check for --cli flag or if arguments are provided (CLI mode)
    bool cliMode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cli") == 0) {
            cliMode = true;
            break;
        }
    }

    // If we have positional arguments (video path), assume CLI mode
    if (argc >= 4 && argv[1][0] != '-') {
        cliMode = true;
    }

#ifdef SHARPCTL_GUI_ENABLED
    if (!cliMode) {
        return runGui();
    }
#endif

    return runCli(argc, argv);
}
