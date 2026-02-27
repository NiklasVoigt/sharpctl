#include "preview_panel.hpp"
#include "../app.hpp"
#include "../widgets/frame_texture.hpp"

#include <imgui.h>

#include <memory>

namespace sharpctl::gui {

// Static texture for preview (persists across frames)
static std::unique_ptr<FrameTexture> s_previewTexture;

void renderPreviewPanel(App& app) {
    ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Preview")) {
        ImGui::End();
        return;
    }

    // Initialize texture on first use
    if (!s_previewTexture) {
        s_previewTexture = std::make_unique<FrameTexture>();
    }

    // Update texture if new frame available
    cv::Mat frame;
    if (app.getPreviewFrame(frame)) {
        s_previewTexture->upload(frame);
    }

    double hoveredTime = app.getHoveredTime();
    const auto& selectedFrames = app.getSelectedFrames();

    // Display preview image
    if (s_previewTexture->isValid()) {
        // Calculate display size maintaining aspect ratio
        ImVec2 avail = ImGui::GetContentRegionAvail();
        avail.y -= 60;  // Reserve space for info text

        float texAspect = static_cast<float>(s_previewTexture->getWidth()) /
                         static_cast<float>(s_previewTexture->getHeight());
        float availAspect = avail.x / avail.y;

        ImVec2 displaySize;
        if (texAspect > availAspect) {
            // Width limited
            displaySize.x = avail.x;
            displaySize.y = avail.x / texAspect;
        } else {
            // Height limited
            displaySize.y = avail.y;
            displaySize.x = avail.y * texAspect;
        }

        // Center the image
        float offsetX = (avail.x - displaySize.x) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        ImGui::Image(static_cast<ImTextureID>(s_previewTexture->getTextureID()),
                     displaySize);

        ImGui::Spacing();

        // Frame info
        if (hoveredTime >= 0.0) {
            // Find sharpness at this time
            double sharpness = 0.0;
            bool isSelected = false;

            // Check if this time matches a selected frame
            for (const auto& sf : selectedFrames) {
                if (std::abs(sf.time - hoveredTime) < 0.1) {
                    sharpness = sf.sharpness;
                    isSelected = sf.selected;
                    break;
                }
            }

            // If not found in selected, estimate from all samples
            if (sharpness == 0.0) {
                const auto& allSamples = app.getAllSamples();
                for (const auto& sample : allSamples) {
                    if (std::abs(sample.time - hoveredTime) < 0.1) {
                        sharpness = sample.sharpness;
                        break;
                    }
                }
            }

            // Time display
            int mins = static_cast<int>(hoveredTime) / 60;
            int secs = static_cast<int>(hoveredTime) % 60;
            int msecs = static_cast<int>((hoveredTime - static_cast<int>(hoveredTime)) * 1000);

            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Time:");
            ImGui::SameLine();
            ImGui::Text("%d:%02d.%03d", mins, secs, msecs);

            ImGui::SameLine(0, 20);

            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Sharpness:");
            ImGui::SameLine();
            ImGui::Text("%.1f", sharpness);

            if (isSelected) {
                ImGui::SameLine(0, 20);
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[SELECTED]");
            }
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f),
                "Hover over timeline to preview frames");
        }
    } else {
        // No preview available
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 textSize = ImGui::CalcTextSize("No preview");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - textSize.x) * 0.5f + ImGui::GetCursorPosX(),
            (avail.y - textSize.y) * 0.5f + ImGui::GetCursorPosY()
        ));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.52f, 1.0f), "No preview");
    }

    ImGui::End();
}

}  // namespace sharpctl::gui
