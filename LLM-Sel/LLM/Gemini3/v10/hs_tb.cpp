#include <iostream>
#include <vector>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "hs_config_params.h"

// Declaration of the HLS function
void hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
);

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <img1> <img2>" << std::endl;
        return -1;
    }

    // Read input images
    cv::Mat img1 = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    cv::Mat img2 = cv::imread(argv[2], cv::IMREAD_GRAYSCALE);

    if (img1.empty() || img2.empty()) {
        std::cerr << "Error loading images" << std::endl;
        return -1;
    }

    if (img1.size() != img2.size()) {
        std::cerr << "Images must have the same size" << std::endl;
        return -1;
    }
    
    // Resize if not 256x128 (Optional, but strictly following prompt 256x128)
    if (img1.cols != MAX_WIDTH || img1.rows != MAX_HEIGHT) {
        std::cout << "Resizing images to " << MAX_WIDTH << "x" << MAX_HEIGHT << std::endl;
        cv::resize(img1, img1, cv::Size(MAX_WIDTH, MAX_HEIGHT));
        cv::resize(img2, img2, cv::Size(MAX_WIDTH, MAX_HEIGHT));
    }

    int width = img1.cols;
    int height = img1.rows;

    // Allocate memory for HLS
    // Using vector for safe allocation
    std::vector<unsigned short> inp1_vec(width * height);
    std::vector<unsigned short> inp2_vec(width * height);
    std::vector<signed short> vx_vec(width * height);
    std::vector<signed short> vy_vec(width * height);

    // Copy data to buffers
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            inp1_vec[i * width + j] = (unsigned short)img1.at<uchar>(i, j);
            inp2_vec[i * width + j] = (unsigned short)img2.at<uchar>(i, j);
        }
    }

    // Call HLS function
    std::cout << "Calling hls_HS..." << std::endl;
    hls_HS(inp1_vec.data(), inp2_vec.data(), vx_vec.data(), vy_vec.data(), height, width);
    std::cout << "hls_HS done." << std::endl;

    // Process output
    cv::Mat vx_mat(height, width, CV_32F);
    cv::Mat vy_mat(height, width, CV_32F);
    cv::Mat flow_vis_vx(height, width, CV_8UC1);
    cv::Mat flow_vis_vy(height, width, CV_8UC1);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            // Raw value is Q8.8
            short raw_vx = vx_vec[i * width + j];
            short raw_vy = vy_vec[i * width + j];
            
            // Convert to float
            float vx = (float)raw_vx / 256.0f;
            float vy = (float)raw_vy / 256.0f;
            
            vx_mat.at<float>(i, j) = vx;
            vy_mat.at<float>(i, j) = vy;

            // Visualization mapping: display = flow * 10 + 128
            int val_vx = (int)(vx * 10.0f + 128.0f);
            int val_vy = (int)(vy * 10.0f + 128.0f);
            
            // Clamp to 0-255
            val_vx = std::max(0, std::min(255, val_vx));
            val_vy = std::max(0, std::min(255, val_vy));
            
            flow_vis_vx.at<uchar>(i, j) = (uchar)val_vx;
            flow_vis_vy.at<uchar>(i, j) = (uchar)val_vy;
        }
    }

    // Save component images
    std::string out_path = "/home/whp/Desktop/XLab/M2HLS/";
    cv::imwrite(out_path + "hls_out_vx.png", flow_vis_vx);
    cv::imwrite(out_path + "hls_out_vy.png", flow_vis_vy);
    
    // Draw Quiver Plot (Arrows)
    // Base image: use img1, convert to BGR
    cv::Mat flow_out_img;
    cv::cvtColor(img1, flow_out_img, cv::COLOR_GRAY2BGR);
    
    int step = 5;
    double arrow_scale = 5.0;
    
    for (int y = 0; y < height; y += step) {
        for (int x = 0; x < width; x += step) {
            float u = vx_mat.at<float>(y, x);
            float v = vy_mat.at<float>(y, x);
            
            // Filter small flow
            if (std::abs(u) < 0.1f && std::abs(v) < 0.1f) continue;
            
            cv::Point p1(x, y);
            cv::Point p2(cvRound(x + u * arrow_scale), cvRound(y + v * arrow_scale));
            
            // Draw arrow
            // Color: Red (BGR 0, 0, 255)
            // Thickness: 2
            // TipLength: 0.3 (relative to arrow length, but OpenCV uses factor relative to arrow length)
            // cv::arrowedLine(img, pt1, pt2, color, thickness, line_type, shift, tipLength)
            cv::arrowedLine(flow_out_img, p1, p2, cv::Scalar(0, 0, 255), 2, 8, 0, 0.3);
        }
    }
    
    cv::imwrite(out_path + "hls_out_flow10.png", flow_out_img);
    std::cout << "Output saved to " << out_path << ": hls_out_vx.png, hls_out_vy.png, hls_out_flow10.png" << std::endl;

    return 0;
}
