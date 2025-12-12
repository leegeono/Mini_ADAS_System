#include "RGB.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <unistd.h>
#include "../TCP_Pi/TCP_Buffer.h"

void* RGB_Thread(void* arg)
{
    cv::VideoCapture cap;

    cap.open(
        "libcamerasrc ! video/x-raw,format=BGR,width=1280,height=720,framerate=30/1 "
        "! videoconvert ! appsink",
        cv::CAP_GSTREAMER
    );

    if (!cap.isOpened())
    {
        std::cerr << "[RGB] Camera open failed" << std::endl;
        return NULL;
    }


    cv::Mat frame;
    cv::Mat resized;     
    cv::Mat letterbox;

    while (1)
    {
        cap >> frame;

        if (frame.empty())
            continue;

        cv::resize(frame, resized, cv::Size(640, 360));

        letterbox = cv::Mat::zeros(640, 640, CV_8UC3);

        resized.copyTo(letterbox(cv::Rect(0, 140, 640, 360)));


        TCP_RGB_Buffer_Push(letterbox.data);

        usleep(1000);
    }

    cap.release();
    return NULL;
}