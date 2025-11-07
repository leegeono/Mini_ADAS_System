#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "Lepton.h"
#include "LeptonSPI.h"


void SaveFrameAsPGM(const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        perror("fopen");
        return;
    }

    // PGM 헤더 작성 (P5 = binary grayscale, 16bit depth)
    fprintf(fp, "P5\n%d %d\n65535\n", FRAME_WIDTH, FRAME_HEIGHT);

    // 픽셀 데이터 기록 (row-major order)
    fwrite(Lepton_Frame, sizeof(uint16_t), FRAME_WIDTH * FRAME_HEIGHT, fp);
    fclose(fp);

    printf("[Capture] Frame saved as %s ✅\n", filename);
}

int main(void)
{
    pthread_t read_tid, process_tid;

    printf("[Capture] Initializing Lepton system...\n");

    Lepton_Reset();
    usleep(300000); // 300ms 안정화 대기

    // 1️⃣ 버퍼 및 SPI 초기화
    PacketBuffer_Init(&PacketBuffer);
    SpiOpenPort(); // SPI0, 10MHz

    // 2️⃣ 스레드 시작
    pthread_create(&read_tid, NULL, ReadThread, NULL);
    pthread_create(&process_tid, NULL, ProcessThread, NULL);

    // 3️⃣ 프레임 완성까지 대기
    printf("[Capture] Waiting for frame data...\n");
    while (!frame_ready)
        usleep(10000); // 10ms polling

    // 4️⃣ 완성된 프레임 저장
    SaveFrameAsPGM("lepton_capture.pgm");

    printf("[Capture] Capture complete. Shutting down...\n");

    // 5️⃣ SPI 포트 닫기 및 종료
    SpiClosePort();
    pthread_cancel(read_tid);
    pthread_cancel(process_tid);

    printf("[Capture] System closed cleanly.\n");
    return 0;
}
