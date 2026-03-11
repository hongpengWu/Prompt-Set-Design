#include "hs_config_params.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <opencv2/opencv.hpp>

// Declaration of HLS function
int hls_HS(
    unsigned short int inp1_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int inp2_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vx_img[MAX_HEIGHT*MAX_WIDTH],
    signed short int   vy_img[MAX_HEIGHT*MAX_WIDTH],
    unsigned short int height,
    unsigned short int width
);

void save_flow_map(const cv::Mat& img, const cv::Mat& u, const cv::Mat& v, const std::string& filename, int step, float scale, float arrow_scale) {
    cv::Mat canvas;
    if (img.channels() == 1) {
        cv::cvtColor(img, canvas, cv::COLOR_GRAY2BGR);
    } else {
        canvas = img.clone();
    }

    for (int y = 0; y < canvas.rows; y += step) {
        for (int x = 0; x < canvas.cols; x += step) {
            float fx = u.at<float>(y, x);
            float fy = v.at<float>(y, x);
            
            // Removed threshold to show all flow vectors
            // if (std::abs(fx) < 0.1 && std::abs(fy) < 0.1) continue;

            cv::Point pt1(x, y);
            cv::Point pt2(cvRound(x + fx * arrow_scale), cvRound(y + fy * arrow_scale));
            
            cv::arrowedLine(canvas, pt1, pt2, cv::Scalar(0, 0, 255), 1, 8, 0, 0.3); 
        }
    }
    cv::imwrite(filename, canvas);
}

int main(int argc, char** argv) {
    std::string img1_path = "/home/whp/Desktop/XLab/M2HLS/lkof_im0_256x128.png";
    std::string img2_path = "/home/whp/Desktop/XLab/M2HLS/lkof_im1_256x128.png";
    
    // Output paths
    std::string out_flow_path = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow3.png";
    std::string out_flow_legacy = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow.png";

    std::cout << "Loading images: " << img1_path << " and " << img2_path << std::endl;

    cv::Mat img1 = cv::imread(img1_path, cv::IMREAD_GRAYSCALE);
    cv::Mat img2 = cv::imread(img2_path, cv::IMREAD_GRAYSCALE);

    if (img1.empty() || img2.empty()) {
        std::cout << "Error loading images." << std::endl;
        return 1;
    }

    int height = img1.rows;
    int width = img1.cols;
    
    if (height != MAX_HEIGHT || width != MAX_WIDTH) {
         std::cout << "Image size mismatch: " << width << "x" << height 
                   << " vs " << MAX_WIDTH << "x" << MAX_HEIGHT << std::endl;
         return 1;
    }

    std::vector<unsigned short> hls_in1(MAX_HEIGHT*MAX_WIDTH);
    std::vector<unsigned short> hls_in2(MAX_HEIGHT*MAX_WIDTH);
    std::vector<signed short>   hls_vx(MAX_HEIGHT*MAX_WIDTH);
    std::vector<signed short>   hls_vy(MAX_HEIGHT*MAX_WIDTH);

    // Prepare inputs
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            hls_in1[i*width + j] = img1.at<uint8_t>(i, j);
            hls_in2[i*width + j] = img2.at<uint8_t>(i, j);
            hls_vx[i*width + j] = 0;
            hls_vy[i*width + j] = 0;
        }
    }

    std::cout << "Calling HLS function..." << std::endl;
    hls_HS(hls_in1.data(), hls_in2.data(), hls_vx.data(), hls_vy.data(), height, width);
    std::cout << "HLS function returned." << std::endl;

    // Convert fixed point output to float for visualization
    cv::Mat u_out(height, width, CV_32F);
    cv::Mat v_out(height, width, CV_32F);
    
    float scale = 1.0f / (1 << FLOW_FRAC_BITS);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            short raw_u = hls_vx[i*width + j];
            short raw_v = hls_vy[i*width + j];
            
            u_out.at<float>(i, j) = (float)raw_u * scale;
            v_out.at<float>(i, j) = (float)raw_v * scale;
        }
    }

    // Save visualization
    // Step 8, Arrow Scale 1.0 (Adjust as needed to match Matlab)
    save_flow_map(img1, u_out, v_out, out_flow_path, 8, 1.0, 3.0);
    save_flow_map(img1, u_out, v_out, out_flow_legacy, 8, 1.0, 3.0); // Save legacy name too just in case

    std::cout << "Saved flow visualization to " << out_flow_path << std::endl;

    return 0;
}
