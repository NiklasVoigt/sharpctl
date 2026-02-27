#include "control_panel.hpp"
#include "../app.hpp"

#include <imgui.h>
#include <implot.h>
#include <portable-file-dialogs.h>

#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace sharpctl::gui {

void renderControlPanel(App& app) {
    ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Controls")) {
        ImGui::End();
        return;
    }

    const auto& videoInfo = app.getVideoInfo();
    auto& params = app.getParams();
    bool isAnalyzing = app.isAnalyzing();

    // Load Video section
    ImGui::SeparatorText("Video");

    ImGui::BeginDisabled(isAnalyzing);
    if (ImGui::Button("Load Video", ImVec2(-1, 0))) {
        auto selection = pfd::open_file("Select video file", ".",
            { "Video files", "*.mp4 *.avi *.mkv *.mov *.webm",
              "All files", "*" }).result();
        if (!selection.empty()) {
            app.loadVideo(selection[0]);
        }
    }
    ImGui::EndDisabled();

    // Video info
    if (videoInfo.isValid()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "File:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", fs::path(videoInfo.path).filename().string().c_str());

        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Duration:");
        ImGui::SameLine();
        int mins = static_cast<int>(videoInfo.duration) / 60;
        int secs = static_cast<int>(videoInfo.duration) % 60;
        ImGui::Text("%d:%02d (%.1f fps)", mins, secs, videoInfo.fps);

        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Resolution:");
        ImGui::SameLine();
        ImGui::Text("%dx%d", videoInfo.width, videoInfo.height);
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "No video loaded");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "Drag & drop a file");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Parameters section
    ImGui::SeparatorText("Parameters");

    ImGui::BeginDisabled(isAnalyzing);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##interval", &params.intervalSec, 0.5f, 30.0f, "Interval: %.1f sec")) {
        // Clamp to valid range
        params.intervalSec = std::max(0.5f, params.intervalSec);
        app.markConfigDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Target time between extracted frames");
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##window", &params.searchWindowSec, 0.0f, 2.0f, "Window: %.2f sec")) {
        app.markConfigDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Search window around each target time (+/-)");
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##step", &params.searchStepSec, 0.01f, 0.5f, "Step: %.2f sec")) {
        params.searchStepSec = std::max(0.01f, params.searchStepSec);
        app.markConfigDirty();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Step size for searching within the window");
    }

    ImGui::Spacing();

    const char* algorithms[] = {
        "FFT (slower, higher quality)",
        "Laplacian (faster, lower quality)"
    };
    int currentAlgo = static_cast<int>(params.algorithm);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##algorithm", &currentAlgo, algorithms, 2)) {
        params.algorithm = static_cast<SharpnessAlgorithm>(currentAlgo);
        app.markConfigDirty();
        // Re-analyze if we already have data
        if (!app.getAllSamples().empty()) {
            app.startAnalysis();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sharpness detection algorithm");
    }

    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Analysis section
    ImGui::SeparatorText("Analysis");

    if (isAnalyzing) {
        // Progress bar
        float progress = app.getProgress();
        ImGui::ProgressBar(progress, ImVec2(-1, 0));

        std::string status = app.getStatusText();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "%s", status.c_str());

        // Remaining time
        float remaining = app.getRemainingSeconds();
        if (remaining > 0) {
            int mins = static_cast<int>(remaining) / 60;
            int secs = static_cast<int>(remaining) % 60;
            if (mins > 0) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Remaining: %d:%02d", mins, secs);
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Remaining: %ds", secs);
            }
        } else if (remaining < 0) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Remaining: estimating...");
        }

        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            app.cancelAnalysis();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Press Escape to cancel");
        }
    } else {
        ImGui::BeginDisabled(!videoInfo.isValid());
        if (ImGui::Button("Analyze Video", ImVec2(-1, 0))) {
            app.startAnalysis();
        }
        ImGui::EndDisabled();

        if (!videoInfo.isValid() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Load a video first");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Export section
    ImGui::SeparatorText("Export");

    int selectedCount = app.getSelectedCount();
    ImGui::TextColored(ImVec4(0.26f, 0.75f, 0.75f, 1.0f), "Selected frames: %d", selectedCount);

    ImGui::BeginDisabled(isAnalyzing || selectedCount == 0);
    if (ImGui::Button("Export Frames", ImVec2(-1, 0))) {
        auto folder = pfd::select_folder("Select output folder", ".").result();
        if (!folder.empty()) {
            app.exportFrames(folder);
        }
    }
    ImGui::EndDisabled();

    if ((isAnalyzing || selectedCount == 0) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (selectedCount == 0) {
            ImGui::SetTooltip("No frames selected. Run analysis first.");
        } else {
            ImGui::SetTooltip("Wait for current operation to complete");
        }
    }

    ImGui::Spacing();

    // Save Config button
    ImGui::BeginDisabled(isAnalyzing || !videoInfo.isValid());
    std::string saveLabel = app.hasUnsavedChanges() ? "Save Config *" : "Save Config";
    if (ImGui::Button(saveLabel.c_str(), ImVec2(-1, 0))) {
        app.saveConfig();
    }
    ImGui::EndDisabled();

    if (!videoInfo.isValid() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Load a video first");
    } else if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Save analysis settings and selected frames");
    }

    ImGui::Spacing();

    // Status
    if (!isAnalyzing) {
        std::string status = app.getStatusText();
        if (!status.empty()) {
            ImGui::TextWrapped("%s", status.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Performance Stats Graph
    ImGui::SeparatorText("Performance");

    const auto& perfStats = app.getPerfStats();

    // Show current values
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CPU:");
    ImGui::SameLine();
    ImGui::Text("%.0f%%", perfStats.currentCpu);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "  GPU:");
    ImGui::SameLine();
    ImGui::Text("%.0f%%", perfStats.currentGpu);

    // Prepare data for plotting (handle ring buffer wrap-around)
    static std::array<float, PerfStats::HISTORY_SIZE> cpuPlot, gpuPlot, xAxis;
    static bool xAxisInit = false;
    if (!xAxisInit) {
        for (size_t i = 0; i < PerfStats::HISTORY_SIZE; i++) {
            xAxis[i] = static_cast<float>(i);
        }
        xAxisInit = true;
    }

    // Copy history in correct order (oldest to newest)
    size_t idx = perfStats.historyIndex;
    for (size_t i = 0; i < PerfStats::HISTORY_SIZE; i++) {
        cpuPlot[i] = perfStats.cpuHistory[idx];
        gpuPlot[i] = perfStats.gpuHistory[idx];
        idx = (idx + 1) % PerfStats::HISTORY_SIZE;
    }

    // Plot both CPU and GPU in one graph
    ImVec2 plotSize(-1, 80);
    if (ImPlot::BeginPlot("##PerfGraph", plotSize,
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                          ImPlotFlags_NoMouseText | ImPlotFlags_NoInputs)) {

        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks |
                          ImPlotAxisFlags_NoGridLines,
                          ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, PerfStats::HISTORY_SIZE - 1, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100, ImPlotCond_Always);

        // CPU line (cyan)
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.5f);
        ImPlot::PlotLine("CPU", xAxis.data(), cpuPlot.data(), PerfStats::HISTORY_SIZE);
        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();

        // GPU line (green)
        ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.4f, 1.0f, 0.5f, 1.0f));
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.5f);
        ImPlot::PlotLine("GPU", xAxis.data(), gpuPlot.data(), PerfStats::HISTORY_SIZE);
        ImPlot::PopStyleVar();
        ImPlot::PopStyleColor();

        ImPlot::EndPlot();
    }

    ImGui::End();
}

}  // namespace sharpctl::gui
