#pragma once

#include <opencv2/opencv.hpp>
#include <GL/gl.h>

namespace sharpctl::gui {

class FrameTexture {
public:
    FrameTexture();
    ~FrameTexture();

    // Upload a cv::Mat to GPU texture
    void upload(const cv::Mat& frame);

    // Clear the texture
    void clear();

    // Get texture ID for ImGui::Image
    GLuint getTextureID() const { return textureID_; }

    // Get dimensions
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Check if texture is valid
    bool isValid() const { return textureID_ != 0 && width_ > 0 && height_ > 0; }

private:
    GLuint textureID_ = 0;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace sharpctl::gui
