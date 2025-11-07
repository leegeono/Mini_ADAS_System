#include "Lepton.h"
#include "LeptonSPI.h"
#include <pthread.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>

using namespace cv;

void DisplayGrayFrame(uint16_t frame[FRAME_HEIGHT][FRAME_WIDTH])
{
    // 1️⃣ Lepton 16비트 데이터를 Mat에 연결
    Mat raw16(FRAME_HEIGHT, FRAME_WIDTH, CV_16UC1, (void *)frame);

    // 2️⃣ 16비트를 8비트로 스케일 변환 (0~65535 → 0~255)
    double minVal, maxVal;
    minMaxLoc(raw16, &minVal, &maxVal);  // 자동 대비 조정
    Mat gray8;
    raw16.convertTo(gray8, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));

    // 3️⃣ 영상 출력
    imshow("Lepton Thermal View (Gray)", gray8);
    waitKey(1);  // 프레임 갱신
}


int main(void)
{
    pthread_t read_tid, process_tid;

    printf("[Capture] Initializing Lepton system...\n");

    PacketBuffer_Init(&PacketBuffer);
    SpiOpenPort();

    pthread_create(&read_tid, NULL, ReadThread, NULL);
    pthread_create(&process_tid, NULL, ProcessThread, NULL);

    printf("[View] Waiting for frame data...\n");

    // 4️⃣ 실시간 영상 루프
    while (1)
    {
        if (frame_ready)
        {
            DisplayGrayFrame(Lepton_Frame);
            frame_ready = 0;  // 다음 프레임 대기
        }
        usleep(10000); // 10ms (CPU 부담 줄이기)
    }

    // 종료 처리
    SpiClosePort();
    pthread_cancel(read_tid);
    pthread_cancel(process_tid);
    return 0;
}