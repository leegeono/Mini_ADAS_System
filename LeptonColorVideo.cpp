#include "Lepton.h"
#include "LeptonSPI.h"
#include <pthread.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>

using namespace cv;

// ✅ Lepton 프레임을 컬러 열화상으로 변환 후 표시
void DisplayColorFrame(uint16_t frame[FRAME_HEIGHT][FRAME_WIDTH])
{
    // 1️⃣ Lepton의 16비트 데이터를 OpenCV Mat으로 래핑
    Mat raw16(FRAME_HEIGHT, FRAME_WIDTH, CV_16UC1, (void *)frame);

    // 2️⃣ 픽셀 범위 자동 조정 (대비 자동화)
    double minVal, maxVal;
    minMaxLoc(raw16, &minVal, &maxVal);

    Mat gray8;
    raw16.convertTo(gray8, CV_8UC1,
                    255.0 / (maxVal - minVal),
                    -minVal * 255.0 / (maxVal - minVal));

    // 3️⃣ 컬러맵 적용 (🔥 INFERNO, 다른 옵션: COLORMAP_JET, COLORMAP_HOT)
    Mat colorized;
    applyColorMap(gray8, colorized, COLORMAP_INFERNO);

    // 4️⃣ 영상 출력
    imshow("Lepton Thermal View (Color)", colorized);
    waitKey(1);
}

int main(void)
{
    pthread_t read_tid, process_tid;

    printf("[Video] Initializing Lepton system...\n");

    // SPI 및 버퍼 초기화
    PacketBuffer_Init(&PacketBuffer);
    SpiOpenPort();

    // Lepton 데이터 읽기 / 처리 스레드 실행
    pthread_create(&read_tid, NULL, ReadThread, NULL);
    pthread_create(&process_tid, NULL, ProcessThread, NULL);

    printf("[Video] Starting real-time thermal stream (Color)...\n");

    // 실시간 영상 루프
    while (1)
    {
        if (frame_ready)
        {
            DisplayColorFrame(Lepton_Frame);
            frame_ready = 0; // 다음 프레임 대기
        }
        usleep(10000); // CPU 점유율 완화 (10ms)
    }

    // 종료 처리
    SpiClosePort();
    pthread_cancel(read_tid);
    pthread_cancel(process_tid);

    printf("[Video] System closed cleanly.\n");
    return 0;
}
