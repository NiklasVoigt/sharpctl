#include "video_analyzer.hpp"
#include <filesystem>
#include <cmath>
#include <atomic>
#include <omp.h>

namespace fs = std::filesystem;

namespace sharpctl {

VideoAnalyzer::~VideoAnalyzer() {
    closeVideo();
}

bool VideoAnalyzer::openVideo(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(capMutex_);

    if (cap_.isOpened()) {
        cap_.release();
    }
    videoInfo_ = VideoInfo{};

    cap_.open(path);
    if (!cap_.isOpened()) {
        return false;
    }

    videoInfo_.path = path;
    videoInfo_.fps = cap_.get(cv::CAP_PROP_FPS);
    videoInfo_.frameCount = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    videoInfo_.width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    videoInfo_.height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));

    if (videoInfo_.fps > 0.0 && videoInfo_.frameCount > 0) {
        videoInfo_.duration = videoInfo_.frameCount / videoInfo_.fps;
    } else {
        videoInfo_.duration = 0.0;
    }

    return true;
}

void VideoAnalyzer::closeVideo() {
    std::lock_guard<std::recursive_mutex> lock(capMutex_);
    if (cap_.isOpened()) {
        cap_.release();
    }
    videoInfo_ = VideoInfo{};
}

namespace {

double calculateSharpnessLaplacian(const cv::Mat& gray) {
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);

    cv::Scalar mean, stddev;
    cv::meanStdDev(lap, mean, stddev);
    return stddev[0] * stddev[0];
}

double calculateSharpnessFFT(const cv::Mat& gray) {
    // Pad to optimal DFT size
    cv::Mat padded;
    int m = cv::getOptimalDFTSize(gray.rows);
    int n = cv::getOptimalDFTSize(gray.cols);
    cv::copyMakeBorder(gray, padded, 0, m - gray.rows, 0, n - gray.cols,
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));

    // Create complex planes
    cv::Mat planes[] = {cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexImg;
    cv::merge(planes, 2, complexImg);

    // DFT
    cv::dft(complexImg, complexImg);
    cv::split(complexImg, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);
    cv::Mat mag = planes[0];

    // Shift quadrants so DC is at center
    int cx = mag.cols / 2;
    int cy = mag.rows / 2;
    cv::Mat q0(mag, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(mag, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(mag, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(mag, cv::Rect(cx, cy, cx, cy));
    cv::Mat tmp;
    q0.copyTo(tmp); q3.copyTo(q0); tmp.copyTo(q3);
    q1.copyTo(tmp); q2.copyTo(q1); tmp.copyTo(q2);

    // Create mask for high frequencies (outer ring)
    cv::Mat mask = cv::Mat::zeros(mag.size(), CV_32F);
    int radius = std::min(cx, cy) / 3;  // Inner radius to exclude (low freq)
    cv::circle(mask, cv::Point(cx, cy), radius, cv::Scalar(0), -1);
    mask.setTo(1.0f);
    cv::circle(mask, cv::Point(cx, cy), radius, cv::Scalar(0), -1);

    // Sum high-frequency magnitudes
    cv::Mat highFreq;
    cv::multiply(mag, mask, highFreq);
    double sum = cv::sum(highFreq)[0];

    // Normalize by image size
    return sum / (gray.rows * gray.cols);
}

}  // anonymous namespace

double VideoAnalyzer::calculateSharpness(const cv::Mat& bgr, SharpnessAlgorithm algo) {
    cv::Mat gray;
    if (bgr.channels() == 3) {
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = bgr;
    }

    switch (algo) {
        case SharpnessAlgorithm::Laplacian:
            return calculateSharpnessLaplacian(gray);
        case SharpnessAlgorithm::FFT:
        default:
            return calculateSharpnessFFT(gray);
    }
}

bool VideoAnalyzer::getFrameAt(double timeSec, cv::Mat& outFrame) {
    std::lock_guard<std::recursive_mutex> lock(capMutex_);
    if (!cap_.isOpened()) return false;

    cap_.set(cv::CAP_PROP_POS_MSEC, timeSec * 1000.0);
    return cap_.read(outFrame);
}

bool VideoAnalyzer::analyzeFullVideo(const AnalysisParams& params,
                                     std::vector<FrameData>& outSamples,
                                     ProgressCallback progressCb,
                                     SampleCallback sampleCb) {
    if (!cap_.isOpened()) return false;

    outSamples.clear();
    resetCancel();

    const double duration = videoInfo_.duration;
    if (duration <= 0.0) return false;

    const double step = static_cast<double>(params.sampleStepSec);

    // Pre-compute sample times
    std::vector<double> sampleTimes;
    for (double t = 0.0; t <= duration; t += step) {
        sampleTimes.push_back(t);
    }

    const size_t totalSamples = sampleTimes.size();
    std::vector<FrameData> results(totalSamples);
    std::atomic<int> completed{0};

    #pragma omp parallel
    {
        // Each thread opens its own VideoCapture (cv::VideoCapture is not thread-safe)
        cv::VideoCapture localCap(videoInfo_.path);

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < totalSamples; ++i) {
            if (isCancelled()) continue;

            double t = sampleTimes[i];
            cv::Mat frame;
            localCap.set(cv::CAP_PROP_POS_MSEC, t * 1000.0);
            if (localCap.read(frame)) {
                results[i].time = t;
                results[i].sharpness = calculateSharpness(frame, params.algorithm);
                results[i].selected = false;
            } else {
                results[i].sharpness = -1.0;  // Mark as invalid
            }

            int done = ++completed;
            #pragma omp critical
            {
                if (progressCb && (done % 10 == 0 || done == static_cast<int>(totalSamples))) {
                    progressCb(static_cast<float>(done) / totalSamples, "Analyzing video...");
                }
            }
        }
    }

    // Collect valid results (samples arrive out-of-order, so no live sampleCb during parallel)
    for (const auto& r : results) {
        if (r.sharpness >= 0.0) {
            outSamples.push_back(r);
            if (sampleCb) {
                sampleCb(r);
            }
        }
    }

    if (progressCb && !isCancelled()) {
        progressCb(1.0f, "Analysis complete");
    }

    return !isCancelled();
}

bool VideoAnalyzer::findOptimalFrames(const AnalysisParams& params,
                                      const std::vector<FrameData>& allSamples,
                                      std::vector<FrameData>& outSelected,
                                      ProgressCallback progressCb,
                                      SearchCallback searchCb) {
    if (!cap_.isOpened()) return false;

    outSelected.clear();
    resetCancel();

    const double duration = videoInfo_.duration;
    if (duration <= 0.0) return false;

    const double interval = static_cast<double>(params.intervalSec);
    const double window = static_cast<double>(params.searchWindowSec);
    const double step = static_cast<double>(params.searchStepSec);

    // Pre-compute target times
    std::vector<double> targetTimes;
    for (double t = 0.0; t <= duration; t += interval) {
        targetTimes.push_back(t);
    }

    const size_t totalTargets = targetTimes.size();
    std::vector<FrameData> results(totalTargets);
    std::atomic<int> completed{0};

    #pragma omp parallel
    {
        // Each thread opens its own VideoCapture
        cv::VideoCapture localCap(videoInfo_.path);

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < totalTargets; ++i) {
            if (isCancelled()) continue;

            double targetT = targetTimes[i];
            double bestVar = -1.0;
            double bestTime = targetT;
            cv::Mat bestFrame;

            const double startT = std::max(0.0, targetT - window);
            const double endT = std::min(duration, targetT + window);

            for (double ts = startT; ts <= endT + 1e-9 && !isCancelled(); ts += step) {
                cv::Mat frame;
                localCap.set(cv::CAP_PROP_POS_MSEC, ts * 1000.0);
                if (localCap.read(frame)) {
                    double v = calculateSharpness(frame, params.algorithm);
                    if (v > bestVar) {
                        bestVar = v;
                        bestTime = ts;
                        bestFrame = frame.clone();
                    }
                }
            }

            if (!bestFrame.empty()) {
                results[i].time = bestTime;
                results[i].sharpness = bestVar;
                results[i].selected = true;

                // Create thumbnail
                const int thumbHeight = 120;
                const int thumbWidth = static_cast<int>(thumbHeight * bestFrame.cols / bestFrame.rows);
                cv::resize(bestFrame, results[i].thumbnail, cv::Size(thumbWidth, thumbHeight));
            }

            int done = ++completed;
            #pragma omp critical
            {
                if (progressCb && (done % 5 == 0 || done == static_cast<int>(totalTargets))) {
                    progressCb(static_cast<float>(done) / totalTargets, "Finding optimal frames...");
                }
            }
        }
    }

    // Collect non-empty results (searchCb disabled during parallel mode)
    for (const auto& r : results) {
        if (!r.thumbnail.empty()) {
            outSelected.push_back(r);
        }
    }

    // Clear search state when done
    if (searchCb) {
        searchCb(0, 0, 0, 0, 0);
    }

    if (progressCb && !isCancelled()) {
        progressCb(1.0f, "Selection complete");
    }

    return !isCancelled();
}

bool VideoAnalyzer::exportFrames(const std::vector<FrameData>& frames,
                                 const std::string& outputDir,
                                 ProgressCallback progressCb) {
    if (!cap_.isOpened()) return false;

    resetCancel();
    fs::create_directories(outputDir);

    // Collect frames to export with their indices
    std::vector<std::pair<size_t, const FrameData*>> toExport;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].selected) {
            toExport.emplace_back(toExport.size(), &frames[i]);
        }
    }

    const size_t totalExport = toExport.size();
    std::atomic<int> completed{0};
    std::atomic<bool> failed{false};

    #pragma omp parallel
    {
        // Each thread opens its own VideoCapture
        cv::VideoCapture localCap(videoInfo_.path);

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < totalExport; ++i) {
            if (isCancelled() || failed.load()) continue;

            size_t index = toExport[i].first;
            const FrameData* fd = toExport[i].second;

            cv::Mat frame;
            localCap.set(cv::CAP_PROP_POS_MSEC, fd->time * 1000.0);
            if (localCap.read(frame)) {
                char filename[512];
                std::snprintf(filename, sizeof(filename),
                              "frame_%04zu_t%.3f_var%.2f.jpg",
                              index, fd->time, fd->sharpness);

                const fs::path outPath = fs::path(outputDir) / filename;
                if (!cv::imwrite(outPath.string(), frame)) {
                    failed.store(true);
                }
            }

            int done = ++completed;
            #pragma omp critical
            {
                if (progressCb && (done % 5 == 0 || done == static_cast<int>(totalExport))) {
                    progressCb(static_cast<float>(done) / totalExport, "Exporting frames...");
                }
            }
        }
    }

    if (failed.load()) {
        return false;
    }

    if (progressCb && !isCancelled()) {
        progressCb(1.0f, "Export complete");
    }

    return !isCancelled();
}

}  // namespace sharpctl
