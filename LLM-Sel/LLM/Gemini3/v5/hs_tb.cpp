#include "hs_config_params.h"
#include <iostream>
#include <vector>
#include <cmath>
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

// Golden Reference
void golden_HS(
    const cv::Mat& im1,
    const cv::Mat& im2,
    cv::Mat& vx,
    cv::Mat& vy,
    int iterations,
    float alpha
) {
    int height = im1.rows;
    int width = im1.cols;
    
    vx = cv::Mat::zeros(height, width, CV_32F);
    vy = cv::Mat::zeros(height, width, CV_32F);
    
    cv::Mat Ix = cv::Mat::zeros(height, width, CV_32F);
    cv::Mat Iy = cv::Mat::zeros(height, width, CV_32F);
    cv::Mat It = cv::Mat::zeros(height, width, CV_32F);

    // Gradients
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int im1_idx = (i > 0) ? i - 1 : 0;
            int ip1_idx = (i < height - 1) ? i + 1 : height - 1;
            int jm1_idx = (j > 0) ? j - 1 : 0;
            int jp1_idx = (j < width - 1) ? j + 1 : width - 1;

            float val_m1_m1 = (float)im1.at<uint8_t>(im1_idx, jm1_idx);
            float val_m1_p1 = (float)im1.at<uint8_t>(im1_idx, jp1_idx);
            float val_0_m1  = (float)im1.at<uint8_t>(i, jm1_idx);
            float val_0_p1  = (float)im1.at<uint8_t>(i, jp1_idx);
            float val_p1_m1 = (float)im1.at<uint8_t>(ip1_idx, jm1_idx);
            float val_p1_p1 = (float)im1.at<uint8_t>(ip1_idx, jp1_idx);
            float val_m1_0  = (float)im1.at<uint8_t>(im1_idx, j);
            float val_p1_0  = (float)im1.at<uint8_t>(ip1_idx, j);

            float gx = -val_m1_m1 + val_m1_p1 - 2*val_0_m1 + 2*val_0_p1 - val_p1_m1 + val_p1_p1;
            float gy = -val_m1_m1 - 2*val_m1_0 - val_m1_p1 + val_p1_m1 + 2*val_p1_0 + val_p1_p1;
            
            Ix.at<float>(i, j) = gx;
            Iy.at<float>(i, j) = gy;
            It.at<float>(i, j) = (float)im2.at<uint8_t>(i, j) - (float)im1.at<uint8_t>(i, j);
        }
    }

    cv::Mat u_curr = cv::Mat::zeros(height, width, CV_32F);
    cv::Mat v_curr = cv::Mat::zeros(height, width, CV_32F);
    cv::Mat u_next = cv::Mat::zeros(height, width, CV_32F);
    cv::Mat v_next = cv::Mat::zeros(height, width, CV_32F);

    for (int k = 0; k < iterations; k++) {
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int im1_idx = (i > 0) ? i - 1 : 0;
                int ip1_idx = (i < height - 1) ? i + 1 : height - 1;
                int jm1_idx = (j > 0) ? j - 1 : 0;
                int jp1_idx = (j < width - 1) ? j + 1 : width - 1;
                
                float u_avg = (u_curr.at<float>(im1_idx, j) + u_curr.at<float>(ip1_idx, j) + 
                               u_curr.at<float>(i, jm1_idx) + u_curr.at<float>(i, jp1_idx)) * 0.25f;
                float v_avg = (v_curr.at<float>(im1_idx, j) + v_curr.at<float>(ip1_idx, j) + 
                               v_curr.at<float>(i, jm1_idx) + v_curr.at<float>(i, jp1_idx)) * 0.25f;
                
                float ix = Ix.at<float>(i, j);
                float iy = Iy.at<float>(i, j);
                float it = It.at<float>(i, j);
                
                float den = alpha*alpha + ix*ix + iy*iy;
                float num = ix*u_avg + iy*v_avg + it;
                
                if (den < 1e-6) den = 1e-6; 

                u_next.at<float>(i, j) = u_avg - ix * (num / den);
                v_next.at<float>(i, j) = v_avg - iy * (num / den);
            }
        }
        u_curr = u_next.clone();
        v_curr = v_next.clone();
    }
    vx = u_curr;
    vy = v_curr;
}

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
            
            // NOTE: Removed thresholding as per requirement
            // if (std::abs(fx) < 0.1 && std::abs(fy) < 0.1) continue;

            cv::Point pt1(x, y);
            // arrow_scale matches MATLAB requirement
            cv::Point pt2(cvRound(x + fx * arrow_scale), cvRound(y + fy * arrow_scale));
            
            // Draw arrow with shift=4 for subpixel accuracy
            cv::arrowedLine(canvas, pt1, pt2, cv::Scalar(0, 0, 255), 1, 8, 4, 0.3); 
        }
    }
    cv::imwrite(filename, canvas);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <img1> <img2>" << std::endl;
        return 1;
    }

    std::string img1_path = argv[1];
    std::string img2_path = argv[2];

    std::cout << "Loading images: " << img1_path << " and " << img2_path << std::endl;

    cv::Mat img1 = cv::imread(img1_path, cv::IMREAD_GRAYSCALE);
    cv::Mat img2 = cv::imread(img2_path, cv::IMREAD_GRAYSCALE);

    if (img1.empty() || img2.empty()) {
        std::cout << "Error loading images." << std::endl;
        return 1;
    }

    int height = img1.rows;
    int width = img1.cols;
    
    std::cout << "Image Size: " << width << "x" << height << std::endl;

    if (height > MAX_HEIGHT || width > MAX_WIDTH) {
         std::cout << "Image size exceeds MAX limits defined in header." << std::endl;
         return 1;
    }

    std::vector<unsigned short> hls_in1(MAX_HEIGHT*MAX_WIDTH);
    std::vector<unsigned short> hls_in2(MAX_HEIGHT*MAX_WIDTH);
    std::vector<signed short>   hls_vx(MAX_HEIGHT*MAX_WIDTH);
    std::vector<signed short>   hls_vy(MAX_HEIGHT*MAX_WIDTH);

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

    // Convert HLS output back to float for verification and plotting
    cv::Mat hls_out_u = cv::Mat::zeros(height, width, CV_32F);
    cv::Mat hls_out_v = cv::Mat::zeros(height, width, CV_32F);
    
    // Scaling factor for Q6.10 is 2^10 = 1024.0
    float scale = 1.0f / 1024.0f;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            signed short raw_u = hls_vx[i*width + j];
            signed short raw_v = hls_vy[i*width + j];
            hls_out_u.at<float>(i, j) = (float)raw_u * scale;
            hls_out_v.at<float>(i, j) = (float)raw_v * scale;
        }
    }

    // Save flow map
    // Params: Step=5, Arrow Scale=5.0
    // Filename: /home/whp/Desktop/XLab/M2HLS/hls_out_flow5.png
    std::string out_filename = "/home/whp/Desktop/XLab/M2HLS/hls_out_flow5.png";
    save_flow_map(img1, hls_out_u, hls_out_v, out_filename, 5, 1.0, 5.0);
    std::cout << "Saved flow map to " << out_filename << std::endl;

    // Golden Reference for Verification
    std::cout << "Running Golden Reference..." << std::endl;
    cv::Mat golden_vx, golden_vy;
    golden_HS(img1, img2, golden_vx, golden_vy, N_ITER, (float)ALPHA_VAL);

    // Verify
    float max_err_u = 0;
    float max_err_v = 0;
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            float err_u = std::abs(hls_out_u.at<float>(i, j) - golden_vx.at<float>(i, j));
            float err_v = std::abs(hls_out_v.at<float>(i, j) - golden_vy.at<float>(i, j));
            if (err_u > max_err_u) max_err_u = err_u;
            if (err_v > max_err_v) max_err_v = err_v;
        }
    }

    std::cout << "Max Error U: " << max_err_u << std::endl;
    std::cout << "Max Error V: " << max_err_v << std::endl;
    
    // Acceptance criteria: Reasonable error due to fixed point
    // Typically < 0.1 is good for flow.
    if (max_err_u < 0.5 && max_err_v < 0.5) {
        std::cout << "Test Passed!" << std::endl;
        return 0;
    } else {
        std::cout << "Test Failed! Error too high." << std::endl;
        return 1;
    }
}
