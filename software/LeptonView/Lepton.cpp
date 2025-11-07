

#include "Lepton.h"       // 우리가 만든 구조체, 매크로 정의
#include "LeptonSPI.h"    // SPI 포트 제어 (spi_cs0_fd, read(), open(), close())
#include <wiringPi.h>     // 랩톤 Reset 을 위한 헤더


// 전역 변수들 초기화  
volatile int frame_ready = 0;
packet_Buf PacketBuffer = {0};
uint16_t Lepton_Frame[FRAME_HEIGHT][FRAME_WIDTH] = {0};


// 공유 버퍼 초기화
void PacketBuffer_Init(packet_Buf *buf) 
{

    // 구조체 내부 초기화
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0; 

    // 버퍼 안의 데이터를 0으로 초기화
    memset(buf->queue, 0, sizeof(buf->queue));

    // 스레드 동기화 객체 초기화
    pthread_mutex_init(&buf->mtx, NULL);
    pthread_cond_init(&buf->cond, NULL);

}


// 공유 버퍼에 데이터 쓰기 동작 
void PacketBuffer_Push(packet_Buf *buf, const uint8_t *input)
{
    pthread_mutex_lock(&buf->mtx);

    while (buf->count >= PACKET_QUEUE_MAX)
        pthread_cond_wait(&buf->cond, &buf->mtx);

    memcpy(buf->queue[buf->tail].packet, input, PACKET_SIZE);
    buf->tail = (buf->tail + 1) % PACKET_QUEUE_MAX;
    buf->count++;

    pthread_cond_signal(&buf->cond);
    pthread_mutex_unlock(&buf->mtx);
}


// 공유 버퍼 데이터 꺼내오기 동작
int PacketBuffer_Pop(packet_Buf *buf, uint8_t *output)
{
    pthread_mutex_lock(&buf->mtx);

    while (buf->count == 0)
        pthread_cond_wait(&buf->cond, &buf->mtx);

    memcpy(output, buf->queue[buf->head].packet, PACKET_SIZE);
    buf->head = (buf->head + 1) % PACKET_QUEUE_MAX;
    buf->count--;

    pthread_cond_signal(&buf->cond);
    pthread_mutex_unlock(&buf->mtx);

    return 0;  // 성공
}


void Lepton_Reset(void)
{
    // wiringPi GPIO 초기화 (처음 한 번만 해도 됨)
    static int initialized = 0;
    if (!initialized) {
        if (wiringPiSetupGpio() == -1) {
            fprintf(stderr, "[Reset] wiringPi init failed!\n");
            return;
        }
        pinMode(LEPTON_RESET_PIN, OUTPUT);
        initialized = 1;
    }

    digitalWrite(LEPTON_RESET_PIN, LOW);
    usleep(RESET_LOW_TIME);
    digitalWrite(LEPTON_RESET_PIN, HIGH);
    usleep(RESET_WAIT_TIME);
    
}



void ProcessFrame(uint8_t segment_buf[4][60][PACKET_SIZE])
{
    int row = 0;

    for (int seg = 0; seg < 4; seg++)
    {
        for (int pk = 0; pk < 60; pk += 2) // 두 패킷 = 한 줄
        {
            uint8_t *left = segment_buf[seg][pk];       // 왼쪽 절반
            uint8_t *right = segment_buf[seg][pk + 1];  // 오른쪽 절반

            // 왼쪽 절반 (Packet0)
            for (int i = 4; i < PACKET_SIZE; i += 2)
            {
                int col = (i - 4) / 2;
                Lepton_Frame[row][col] =
                    ((left[i] << 8) | left[i + 1]);
            }

            // 오른쪽 절반 (Packet1)
            for (int i = 4; i < PACKET_SIZE; i += 2)
            {
                int col = 80 + (i - 4) / 2;
                Lepton_Frame[row][col] =
                    ((right[i] << 8) | right[i + 1]);
            }

            row++; // 두 패킷 합쳐서 한 줄 완성
        }
    }

    frame_ready = 1;
}

void *ReadThread(void *arg)
{
    uint8_t packet[PACKET_SIZE];
    int ret;

    while (1)
    {
        ret = SpiReadPacket(packet);

        if (ret == 0)
        {
            PacketBuffer_Push(&PacketBuffer, packet);
        }
        else
        {
            usleep(1000);
        }
    }
    return NULL;
}


void *ProcessThread(void *arg)
{   
    uint8_t packet[PACKET_SIZE];
    uint8_t segment_buf[4][60][PACKET_SIZE];
    static int Reset_count1 = 0;
    static int Reset_count2 = 0;
    int expected_pk = 0;
    int expected_seg = 1;
    int synced = 0;

    while (1)
    {
        // 1️⃣ 버퍼에서 패킷 하나 꺼냄
        PacketBuffer_Pop(&PacketBuffer, packet);

        // 2️⃣ 버림 패킷 확인 
        if ((packet[0] & 0x0F) == 0x0F)
        {
            Reset_count1++;
            if (Reset_count1 > 1400) 
            {             
                Lepton_Reset();       
                Reset_count1 = 0;
                synced = 0;
                expected_pk = 0;
                expected_seg = 1;
                usleep(1000000);      // 1초 대기 (리셋 안정화)
            }
            continue;
        }
        else 
        {
            Reset_count1 = 0; // 정상 패킷 오면 카운터 리셋
        }

        // 3️⃣ pk_num 추출 (packet[1])
        uint8_t pk_num = packet[1];

        // 4️⃣ 아직 동기화 안 된 상태면 첫 시작 패킷 탐색
        if (!synced)
        {
            if (pk_num == 0)
            {
                synced = 1;
                expected_seg = 1;
                expected_pk = 0;
            }
            else
                continue;
        }

        // 5️⃣ 예상 패킷 번호 불일치 → 동기화 해제 후 재탐색
        if (pk_num != expected_pk)
        {   
            Reset_count2++;
            synced = 0;
            expected_pk = 0;
            expected_seg = 1;

            if (Reset_count2 > 500) 
            { 
                Lepton_Reset();     
                Reset_count2 = 0;
                usleep(1000000);      
            }
            continue;
               
        }
        

        // 6️⃣ pk_num == 20일 때만 seg_id 검사
        if (pk_num == 20)
        {
            uint8_t seg_id = (packet[0] & 0xF0) >> 4;
            if (seg_id != expected_seg)
            {
                synced = 0;
                expected_pk = 0;
                expected_seg = 1;
                continue;
            }
        }

        // 7️⃣ 정상 패킷 저장
        memcpy(segment_buf[expected_seg - 1][pk_num], packet, PACKET_SIZE);
        expected_pk++;

        // 8️⃣ 세그먼트 완성 → 다음 세그먼트로
        if (expected_pk >= 60)
        {
            expected_pk = 0;
            expected_seg++;
        }

        // 9️⃣ 4세그먼트 완성 → 프레임 조립
        if (expected_seg > 4)
        {
            ProcessFrame(segment_buf);
            frame_ready = 1;

            expected_seg = 1;
            expected_pk = 0;
            synced = 0;
            Reset_count1 = 0;
            Reset_count2 = 0;
            
        }
    }

    return NULL;
}



