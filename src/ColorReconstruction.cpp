#pragma once

#include "ColorReconstruction.h"
#include "aruco_samples_utility.hpp"
#include "PoseEstimation.h"
#include "Segmentation.h"

static cv::Vec3f worldToCamera(cv::Vec4f world, cv::Mat& pose, cv::Mat& intr) {
    cv::Mat1f proj = intr * pose * world;
    return cv::Vec3f(proj(0) / proj(2), proj(1) / proj(2), 1);
}

void reconstructColor(cv::Mat& cameraMatrix, cv::Mat& distCoeffs, Model& model, std::vector<cv::Mat>& images, std::vector<cv::Mat>& masks) {
    std::cout << "LOG - CR: starting color reconstruction." << std::endl;

    cv::Mat intr = cameraMatrix.clone();
    intr.convertTo(intr, CV_32F);

    // Estimate pose for each image and remove distortion from images/masks
    std::vector<cv::Vec4f> cameras;
    std::vector<cv::Mat> poses;
    std::vector<cv::Mat> undist_imgs;
    std::vector<cv::Mat> undist_masks;
    for (int i = 0; i < images.size(); i++)
    {
        cv::Mat pose = estimatePoseFromImage(cameraMatrix, distCoeffs, images[i], false);
        poses.push_back(pose.inv()(cv::Rect(0, 0, 4, 3)));
        cameras.push_back(cv::Vec4f(poses[i].at<float>(0, 3), poses[i].at<float>(1, 3), poses[i].at<float>(2, 3), 1));
        cv::Mat undist_img;
        cv::undistort(images[i], undist_img, cameraMatrix, distCoeffs);
        cv::Mat undist_rgb_img;
        cv::cvtColor(undist_img, undist_rgb_img, cv::COLOR_BGR2RGB);
        undist_imgs.push_back(undist_rgb_img);
        cv::Mat undist_mask;
        cv::undistort(masks[i], undist_mask, cameraMatrix, distCoeffs);
        undist_masks.push_back(undist_mask);
    }

    cv::Rect image_borders = cv::Rect(0, 0, images[0].cols, images[0].rows);

    for (int x = 0; x < model.getX(); x++)
    {
        for (int y = 0; y < model.getY(); y++)
        {
            for (int z = 0; z < model.getZ(); z++)
            {
                if (model.get(x, y, z)(3) == 0 ||  model.isInner(x, y, z))
                {
                    continue;
                }
                cv::Vec4f word_coord = model.toWord(x, y, z);
                for (int i = 0; i < undist_imgs.size(); i++) {
                    cv::Vec3f camera_coord = worldToCamera(word_coord, poses[i], intr);
                    cv::Point pixel_pos = cv::Point((int)std::round(camera_coord[0]), (int)std::round(camera_coord[1]));
                    if (!pixel_pos.inside(image_borders))
                    {
                        continue;
                    }
                    cv::Vec3b pixel = undist_masks[i].at<cv::Vec3b>(pixel_pos);
                    if (pixel(0) == 0 && pixel(1) == 0 && pixel(2) == 0) // masked pixel -> set alpha = 0
                    {
                        continue;
                    }
                    pixel = undist_imgs[i].at<cv::Vec3b>(pixel_pos);
                    model.addColor(x, y, z, Eigen::Vector4f(pixel(0), pixel(1), pixel(2), 1), cv::norm(cameras[i] - word_coord));
                }
                std::vector<DCLR> colors = model.getColors(x, y, z);
                if (colors.size() == 0) {
                    continue;
                }
                DCLR min = colors[0];
                for (int i = 1; i < colors.size(); i++) {
                    if (colors[i].depth < min.depth)
                    {
                        min = colors[i];
                    }
                }
                model.set(x, y, z, min.color);
            }
        }
    }

    std::cout << "LOG - CR: color reconstruction finished." << std::endl;
}