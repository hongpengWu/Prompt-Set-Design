#include "hs_config_params.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

// HLS Function Declaration
void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
);

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <img1> <img2>" << std::endl;
        return 1;
    }

    std::string img1_path = argv[1];
    std::string img2_path = argv[2];
    
    // Output path: /home/whp/Desktop/XLab/M2HLS/
    std::string out_dir = "/home/whp/Desktop/XLab/M2HLS/";
    
    // Read images
    cv::Mat img1 = cv::imread(img1_path, cv::IMREAD_GRAYSCALE);
    cv::Mat img2 = cv::imread(img2_path, cv::IMREAD_GRAYSCALE);

    if (img1.empty() || img2.empty()) {
        std::cerr << "Error: Could not read images from " << img1_path << " or " << img2_path << std::endl;
        return 1;
    }

    if (img1.size() != img2.size()) {
        std::cerr << "Error: Images must be same size." << std::endl;
        return 1;
    }

    int height = img1.rows;
    int width = img1.cols;

    if (height > MAX_HEIGHT || width > MAX_WIDTH) {
        std::cerr << "Error: Image size (" << width << "x" << height << ") exceeds maximum (" << MAX_WIDTH << "x" << MAX_HEIGHT << ")." << std::endl;
        return 1;
    }

    std::cout << "Image size: " << width << "x" << height << std::endl;

    // Allocate buffers
    // Use std::vector for safety and automatic cleanup
    std::vector<unsigned short> inp1_vec(MAX_HEIGHT * MAX_WIDTH);
    std::vector<unsigned short> inp2_vec(MAX_HEIGHT * MAX_WIDTH);
    std::vector<signed short>   vx_vec(MAX_HEIGHT * MAX_WIDTH);
    std::vector<signed short>   vy_vec(MAX_HEIGHT * MAX_WIDTH);

    // Copy image data to input buffers
    // Note: img.at<uchar>(y, x) returns 8-bit value
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            inp1_vec[i * width + j] = (unsigned short)img1.at<uchar>(i, j);
            inp2_vec[i * width + j] = (unsigned short)img2.at<uchar>(i, j);
        }
    }

    // Call HLS Kernel
    std::cout << "Calling hls_HS..." << std::endl;
    hls_HS(inp1_vec.data(), inp2_vec.data(), vx_vec.data(), vy_vec.data(), height, width);
    std::cout << "hls_HS done." << std::endl;

    // Process output
    cv::Mat flow_u(height, width, CV_32F);
    cv::Mat flow_v(height, width, CV_32F);

    // Q8.8 scaling factor
    const float scale_factor = 1.0f / 256.0f;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            short raw_u = vx_vec[i * width + j];
            short raw_v = vy_vec[i * width + j];
            
            flow_u.at<float>(i, j) = (float)raw_u * scale_factor;
            flow_v.at<float>(i, j) = (float)raw_v * scale_factor;
        }
    }

    // Visualization 1: Component Maps
    // display = flow * 10 + 128
    cv::Mat vx_display, vy_display;
    flow_u.convertTo(vx_display, CV_8U, 10.0, 128.0);
    flow_v.convertTo(vy_display, CV_8U, 10.0, 128.0);

    cv::imwrite(out_dir + "hls_out_vx.png", vx_display);
    cv::imwrite(out_dir + "hls_out_vy.png", vy_display);

    // Visualization 2: Quiver Plot (Arrows)
    // hls_out_flow9.png
    cv::Mat canvas;
    if (img1.channels() == 1) {
        cv::cvtColor(img1, canvas, cv::COLOR_GRAY2BGR);
    } else {
        canvas = img1.clone();
    }

    int step = 5;
    double arrow_scale = 5.0;
    double threshold = 0.1;

    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            float u = flow_u.at<float>(y, x);
            float v = flow_v.at<float>(y, x);

            if (std::abs(u) < threshold && std::abs(v) < threshold) continue;

            cv::Point pt1(x, y);
            cv::Point pt2(cvRound(x + u * arrow_scale), cvRound(y + v * arrow_scale));

            // Clip to image boundaries
            pt2.x = std::max(0, std::min(width - 1, pt2.x));
            pt2.y = std::max(0, std::min(height - 1, pt2.y));

            cv::arrowedLine(canvas, pt1, pt2, cv::Scalar(0, 0, 255), 2, 8, 0, 0.3);
        }
    }

    cv::imwrite(out_dir + "hls_out_flow9.png", canvas);
    
    std::cout << "Outputs saved to " << out_dir << std::endl;

    return 0;
}
