#include "Lepton.h"
#include "LeptonSPI.h"
#include <pthread.h>
#include <opencv2/opencv.hpp>

uint8_t Video[FRAME_HEIGHT][FRAME_WIDTH][3];

int main()
{   
    Lepton_Reset();
    usleep(1000000);
    PacketBuffer_Init(&PacketBuffer);
    SpiOpenPort();

    pthread_t r, p;
    pthread_create(&r, NULL, ReadThread, NULL);
    pthread_create(&p, NULL, ProcessThread, NULL);

    while (1)
    {
        if (frame_ready)
        {
            Normalize(Lepton_Frame, Video);

            cv::Mat frame8(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3, Video);
            cv::Mat up;
            resize(frame8, up, cv::Size(), 4, 4, cv::INTER_NEAREST);

            imshow("Lepton View", up);
            cv::waitKey(1);

            frame_ready = 0;
        }
    }
}