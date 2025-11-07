#include "Lepton.h"
#include "LeptonSPI.h"
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <string.h>

using namespace cv;

void SaveFrameAsPGM(const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        perror("fopen");
        return;
    }

    fprintf(fp, "P5\n%d %d\n65535\n", FRAME_WIDTH, FRAME_HEIGHT);
    fwrite(Lepton_Frame, sizeof(uint16_t), FRAME_WIDTH * FRAME_HEIGHT, fp);
    fclose(fp);

    printf("[Capture] Frame saved as %s ✅\n", filename);
}

void SaveFrameAsColor(const char *filename)
{
    // 1️⃣ Lepton의 16비트 데이터를 OpenCV Mat으로 읽기
    Mat raw16(FRAME_HEIGHT, FRAME_WIDTH, CV_16UC1, (void *)Lepton_Frame);

    // 2️⃣ 16비트를 8비트로 정규화 (0~65535 → 0~255)
    Mat gray8;
    raw16.convertTo(gray8, CV_8UC1, 1.0 / 256.0);

    // 3️⃣ 컬러맵 적용 (열화상 효과)
    Mat colorized;
    applyColorMap(gray8, colorized, COLORMAP_INFERNO); // 🔥 INFERNO 추천

    // 4️⃣ 컬러 이미지 파일로 저장 (jpg/png 등)
    imwrite(filename, colorized);

    printf("[Capture] Color frame saved as %s ✅\n", filename);
}

int main(void)
{
    pthread_t read_tid, process_tid;

    printf("[Capture] Initializing Lepton system...\n");


    PacketBuffer_Init(&PacketBuffer);
    SpiOpenPort();

    pthread_create(&read_tid, NULL, ReadThread, NULL);
    pthread_create(&process_tid, NULL, ProcessThread, NULL);

    printf("[Capture] Waiting for frame data...\n");
    while (!frame_ready)
        usleep(10000); // 프레임 완성 대기

    // ✅ 흑백/컬러 둘 다 저장 가능
    SaveFrameAsPGM("lepton_capture.pgm");
    SaveFrameAsColor("lepton_capture_color.jpg");

    printf("[Capture] Capture complete. Shutting down...\n");

    SpiClosePort();
    pthread_cancel(read_tid);
    pthread_cancel(process_tid);

    printf("[Capture] System closed cleanly.\n");
    return 0;
}