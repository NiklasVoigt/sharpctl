#include "frame_texture.hpp"

namespace sharpctl::gui {

FrameTexture::FrameTexture() {
    glGenTextures(1, &textureID_);
}

FrameTexture::~FrameTexture() {
    if (textureID_ != 0) {
        glDeleteTextures(1, &textureID_);
    }
}

void FrameTexture::upload(const cv::Mat& frame) {
    if (frame.empty()) {
        clear();
        return;
    }

    // Convert BGR to RGB
    cv::Mat rgb;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    } else if (frame.channels() == 4) {
        cv::cvtColor(frame, rgb, cv::COLOR_BGRA2RGB);
    } else {
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
    }

    width_ = rgb.cols;
    height_ = rgb.rows;

    glBindTexture(GL_TEXTURE_2D, textureID_);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb.data);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void FrameTexture::clear() {
    width_ = 0;
    height_ = 0;
    // Don't delete texture, just mark as invalid by setting dimensions to 0
}

}  // namespace sharpctl::gui
