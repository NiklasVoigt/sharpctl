#include "timeline_panel.hpp"
#include "../app.hpp"

#include <imgui.h>
#include <implot.h>

#include <vector>
#include <cmath>

namespace sharpctl::gui {

namespace {

const char* getAlgorithmName(SharpnessAlgorithm algo) {
    switch (algo) {
        case SharpnessAlgorithm::Laplacian: return "Sharpness (Laplacian)";
        case SharpnessAlgorithm::FFT:
        default: return "Sharpness (FFT)";
    }
}

}  // anonymous namespace

void renderTimelinePanel(App& app) {
    ImGui::SetNextWindowSize(ImVec2(800, 300), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Timeline")) {
        ImGui::End();
        return;
    }

    const auto& allSamples = app.getAllSamples();
    auto& selectedFrames = app.getSelectedFrames();
    const auto& videoInfo = app.getVideoInfo();
    const auto& params = app.getParams();

    if (allSamples.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f),
            "No analysis data. Load a video and click Analyze.");
        ImGui::End();
        return;
    }

    // Prepare data arrays for ImPlot
    std::vector<double> times, sharpness;
    times.reserve(allSamples.size());
    sharpness.reserve(allSamples.size());

    double maxSharpness = 0.0;
    for (const auto& sample : allSamples) {
        times.push_back(sample.time);
        sharpness.push_back(sample.sharpness);
        maxSharpness = std::max(maxSharpness, sample.sharpness);
    }

    // Prepare selected frame markers
    std::vector<double> selectedTimes, selectedSharpness;
    for (const auto& frame : selectedFrames) {
        if (frame.selected) {
            selectedTimes.push_back(frame.time);
            selectedSharpness.push_back(frame.sharpness);
        }
    }

    // Calculate plot dimensions, reserving space for help text
    ImVec2 plotSize = ImGui::GetContentRegionAvail();
    float textHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    plotSize.y = std::max(plotSize.y - textHeight, 150.0f);

    // ImPlot setup
    if (ImPlot::BeginPlot("##SharpnessTimeline", plotSize,
                          ImPlotFlags_NoTitle | ImPlotFlags_Crosshairs)) {

        // Axis setup
        ImPlot::SetupAxes("Time (seconds)", getAlgorithmName(params.algorithm),
                          ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, videoInfo.duration, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, maxSharpness * 1.1, ImPlotCond_Once);

        // Style for sharpness line
        ImPlot::PlotLine("Sharpness", times.data(), sharpness.data(),
                         static_cast<int>(times.size()),
                         ImPlotSpec(ImPlotProp_LineColor, ImVec4(0.26f, 0.75f, 0.75f, 1.0f)));

        // Style for selected frame markers
        if (!selectedTimes.empty()) {
            ImPlot::PlotScatter("Selected", selectedTimes.data(), selectedSharpness.data(),
                               static_cast<int>(selectedTimes.size()),
                               ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Circle,
                                          ImPlotProp_MarkerSize, 8.0f,
                                          ImPlotProp_MarkerFillColor, ImVec4(0.3f, 0.9f, 0.4f, 1.0f),
                                          ImPlotProp_LineWeight, 2.0f,
                                          ImPlotProp_MarkerLineColor, ImVec4(0.2f, 0.7f, 0.3f, 1.0f)));
        }

        // Visualize search window during analysis
        SearchState searchState = app.getSearchState();
        if (searchState.active) {
            double yMax = maxSharpness * 1.1;

            // Draw search window as shaded region
            double windowX[4] = {searchState.windowStart, searchState.windowEnd,
                                 searchState.windowEnd, searchState.windowStart};
            double windowY[4] = {0, 0, yMax, yMax};
            ImPlot::PlotShaded("##searchWindow", windowX, windowY, 4, 0,
                               ImPlotSpec(ImPlotProp_FillColor, ImVec4(1.0f, 0.8f, 0.2f, 0.15f)));

            // Draw window boundaries
            double boundX1[2] = {searchState.windowStart, searchState.windowStart};
            double boundX2[2] = {searchState.windowEnd, searchState.windowEnd};
            double boundY[2] = {0, yMax};
            ImPlot::PlotLine("##winStart", boundX1, boundY, 2,
                             ImPlotSpec(ImPlotProp_LineColor, ImVec4(1.0f, 0.8f, 0.2f, 0.6f), ImPlotProp_LineWeight, 1.5f));
            ImPlot::PlotLine("##winEnd", boundX2, boundY, 2,
                             ImPlotSpec(ImPlotProp_LineColor, ImVec4(1.0f, 0.8f, 0.2f, 0.6f), ImPlotProp_LineWeight, 1.5f));

            // Draw current search position
            double searchX[2] = {searchState.currentSearchTime, searchState.currentSearchTime};
            double searchY[2] = {0, yMax};
            ImPlot::PlotLine("##searchPos", searchX, searchY, 2,
                             ImPlotSpec(ImPlotProp_LineColor, ImVec4(1.0f, 0.4f, 0.1f, 0.9f), ImPlotProp_LineWeight, 2.0f));

            // Draw current best found (if any)
            if (searchState.bestSharpness > 0) {
                double bestX[1] = {searchState.bestTime};
                double bestY[1] = {searchState.bestSharpness};
                ImPlot::PlotScatter("##bestFound", bestX, bestY, 1,
                                   ImPlotSpec(ImPlotProp_Marker, ImPlotMarker_Diamond,
                                              ImPlotProp_MarkerSize, 10.0f,
                                              ImPlotProp_MarkerFillColor, ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                                              ImPlotProp_LineWeight, 2.0f,
                                              ImPlotProp_MarkerLineColor, ImVec4(1.0f, 0.5f, 0.5f, 1.0f)));
            }
        }

        // Handle mouse interactions
        if (ImPlot::IsPlotHovered()) {
            ImPlotPoint mousePos = ImPlot::GetPlotMousePos();
            double hoveredTime = mousePos.x;

            // Clamp to valid range
            hoveredTime = std::max(0.0, std::min(hoveredTime, videoInfo.duration));
            app.setHoveredTime(hoveredTime);

            // Update preview frame on hover
            static double lastPreviewTime = -1.0;
            if (std::abs(hoveredTime - lastPreviewTime) > 0.05) {
                lastPreviewTime = hoveredTime;
                cv::Mat frame;
                if (app.getAnalyzer().getFrameAt(hoveredTime, frame)) {
                    app.setPreviewFrame(frame);
                }
            }

            // Draw vertical indicator line at cursor
            double indicatorY[2] = {0.0, maxSharpness * 1.1};
            double indicatorX[2] = {hoveredTime, hoveredTime};
            ImPlot::PlotLine("##indicator", indicatorX, indicatorY, 2,
                             ImPlotSpec(ImPlotProp_LineColor, ImVec4(1.0f, 1.0f, 1.0f, 0.5f), ImPlotProp_LineWeight, 1.0f));

            // Left-click: toggle selection on nearby marker
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const double clickThreshold = videoInfo.duration * 0.01;  // 1% of duration
                int closestIdx = -1;
                double closestDist = clickThreshold;

                for (int i = 0; i < static_cast<int>(selectedFrames.size()); i++) {
                    double dist = std::abs(selectedFrames[i].time - hoveredTime);
                    if (dist < closestDist) {
                        closestDist = dist;
                        closestIdx = i;
                    }
                }

                if (closestIdx >= 0) {
                    app.toggleFrameSelection(closestIdx);
                }
            }

            // Right-click: add new frame at this position
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                app.addFrameAtTime(hoveredTime);
            }
        } else {
            app.setHoveredTime(-1.0);
        }

        ImPlot::EndPlot();
    }

    // Help text
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f),
        "Left-click marker: toggle selection | Right-click: add frame | Scroll: zoom");

    ImGui::End();
}

}  // namespace sharpctl::gui
