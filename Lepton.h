#ifndef Lepton_H
#define Lepton_H

#include <pthread.h>
#include <stdint.h> 
/*

(단일 Thread 구현)
read()와 패킷 해석(세그먼트 조립, 프레임 변환)을 번갈아 수행하면, 해석 중에 CPU가 점유되어 SPI read가 지연될 수 있다.

(2Thread 구현) 
ReadeThread  → SPI로부터 패킷을 '끊임없이' 읽어 내부 버퍼에 저장
ProcessThread  → Reade가 쌓아둔 버퍼를 해석하여 세그먼트 및 프레임으로 조립
이렇게 함으로써 Lepton 내부 DMA 버퍼 overflow로 인한 패킷 손실을 방지하고 향후 다른 영상처리나 멀티센서 로직이 CPU를 점유하더라도 SPI 수신은 실시간으로 지속될 수 있음.

*/


/* =================== Lepton 관련 상수 정의 ==================== */
#define LEPTON_RESET_PIN 22        // Reset PIN -> GPIO 22번
#define RESET_LOW_TIME   50000     // Low 신호 유지 시간 5ms
#define RESET_WAIT_TIME  500000    // Lepton 안정화 시간 0.5s
#define PACKET_SIZE       164      // Lepton 한 패킷 크기 (Raw 14 ,Telemetry Mode OFF)
#define PACKET_QUEUE_MAX  512      // 공유 버퍼 크기 (SpiReadPacke()이 읽어온 데이터를 저장할 공간) -> 164바이트 패킷 512개 = 2프레임 분량 
#define FRAME_WIDTH       160      // Lepton 영상 너비
#define FRAME_HEIGHT      120      // Lepton 영상 높이
#define SEGMENTS_PER_FRAME 4       // 한 프레임의 세그먼트 수
#define PACKETS_PER_SEGMENT 60     // 세그먼트 당 패킷 수
/* ============================================================ */


/* ======================== 패킷 구조체 ======================== */
typedef struct 
{
    uint8_t packet[PACKET_SIZE];  // SPI로 읽은 패킷 데이터 
} Packet_t;
/* ============================================================ */


/* ===================== 패킷 버퍼 구조체 ====================== */
typedef struct 
{
    Packet_t queue[PACKET_QUEUE_MAX];  // FIFO 버퍼
    int head;                          // 읽기 위치
    int tail;                          // 쓰기 위치
    int count;                         // 현재 저장된 패킷 수
    pthread_mutex_t mtx;               // 스레드 동기화용 뮤텍스
    pthread_cond_t cond;               // 조건 변수 (empty/full 알림)
} packet_Buf;
/* ============================================================ */


/* ======================== 프레임 버퍼 ======================== */

extern uint16_t Lepton_Frame[FRAME_HEIGHT][FRAME_WIDTH];  

/* ============================================================ */


/* ==================== 전역 PacketBuffer ===================== */

extern packet_Buf PacketBuffer ;  // Thread가 공유하는 버퍼
extern volatile int frame_ready;  // 프레임 성공 여부 판단

/* ============================================================ */


/* ===== 동작 관련 함수 선언부 ===== */

void PacketBuffer_Init(packet_Buf *buf);
void PacketBuffer_Push(packet_Buf *buf, const uint8_t *data);
int PacketBuffer_Pop(packet_Buf *buf, uint8_t *output);
void Lepton_Reset(void);
void ProcessFrame(uint8_t segment_buf[4][60][PACKET_SIZE]);
void *ReadThread(void *arg);
void *ProcessThread(void *arg);

/* ============================== */


#endif  // LEPTON_H