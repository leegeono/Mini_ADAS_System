

#include "Lepton.h"      
#include "LeptonSPI.h"   
#include  <wiringPi.h>    // Reset 을 위한 헤더
#include "../TCP_Pi/TCP_Buffer.h" // Lepton 카메라의 데이터를 보내주기 위한 헤더
#include <opencv2/opencv.hpp>

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
    // 사용시 다른 Thread 접근 못하게 lock
    pthread_mutex_lock(&buf->mtx); 

    while (buf->count >= PACKET_QUEUE_MAX)
        pthread_cond_wait(&buf->cond, &buf->mtx); // 공유 버퍼가 가득 차면 기다리기

    // 공유 버퍼로 패킷을 push 하고 다음에 쓸 위치를 저장 + 패킷 수 +1
    memcpy(buf->queue[buf->tail].packet, input, PACKET_SIZE); 
    buf->tail = (buf->tail + 1) % PACKET_QUEUE_MAX;
    buf->count++;
    
    // ProcessThread 신호 인가 후 lock 해제 , 즉 읽어 왔으니 해석해라 명령
    pthread_cond_signal(&buf->cond); 
    pthread_mutex_unlock(&buf->mtx);
}


// 공유 버퍼 데이터 꺼내오기 동작
int PacketBuffer_Pop(packet_Buf *buf, uint8_t *output)
{   
    // 사용시 다른 Thread 접근 못하게 lock
    pthread_mutex_lock(&buf->mtx); 

    // 공유 버퍼에 아무 패킷도 없다면 기다리기 
    while (buf->count == 0)
        pthread_cond_wait(&buf->cond, &buf->mtx);

    // 공유 버퍼에서 패킷을 pop 하고 다음에 꺼낼위치를 저장 + 패킷 수 -1
    memcpy(output, buf->queue[buf->head].packet, PACKET_SIZE);
    buf->head = (buf->head + 1) % PACKET_QUEUE_MAX;
    buf->count--;

    // ReadThread 신호 인가 후 lock 해제 , 즉 꺼냈으니 다시 넣어라 명령
    pthread_cond_signal(&buf->cond);
    pthread_mutex_unlock(&buf->mtx);

    return 0;  // 성공
}

// Lepton 리셋 하는 함수 -> 치명적 오류 발생시 Reset
void Lepton_Reset(void)
{
    // wiringPi GPIO 초기화 
    static int initialized = 0;
    if (!initialized) 
    {
        if (wiringPiSetupGpio() == -1) // 실패 시
        {
            fprintf(stderr, "[Reset] wiringPi init failed!\n"); // 실패 문구 출력
            return;
        }
        pinMode(LEPTON_RESET_PIN, OUTPUT); // GPIO 22를 출력으로 설정
        initialized = 1;
    }

    digitalWrite(LEPTON_RESET_PIN, LOW); // 22번 핀에 LOW 신호 
    usleep(RESET_LOW_TIME);              // RESET_LOW_TIME 만큼 LOW -> Lepton 권장 시간
    digitalWrite(LEPTON_RESET_PIN, HIGH);// 22번 핀에 HIGH 신호
    usleep(RESET_WAIT_TIME);             // RESET_WAIT_TIME 만큼 대기 -> Lepton 권장 시간
    
}


// ProcessThread 에서 만든 segment 1~4 를 조립해 Lepton_Frame 에 저장
void ProcessFrame(uint8_t segment_buf[4][60][PACKET_SIZE])
{
    int row = 0;

    for (int seg = 0; seg < 4; seg++)
    {
        for (int pk = 0; pk < 60; pk += 2) // 패킷 당 픽셀 80개 -> 160 * 120 해상도에서 패킷 두개가 한 줄을 의미함
        {
            uint8_t *left = segment_buf[seg][pk];       // 왼쪽 패킷 (0,2,4.. 짝수 pk_num)
            uint8_t *right = segment_buf[seg][pk + 1];  // 오른쪽 패킷 (1,3,5.. 홀수 pk_num)

            // 한 패킷은 164 바이트 근데 첫 4바이트는 헤더 이고 나머지 160바이트는 픽셀 데이터 , 픽셀은 하나당 2바이트이기 때문에 /2 수행
            for (int i = 4; i < PACKET_SIZE; i += 2) // 한 픽셀씩 읽기 위해 +2
            {
                int col = (i - 4) / 2; // 픽셀 데이터 저장 위치 -> (i-4) 는 첫 헤더 4바이트 빼기 
                                       // /2 는 픽셀 하나당 2바이트 이기 때문에 160 * 120 에서 160에 1개씩 픽셀 데이터 채우기 수행
                Lepton_Frame[row][col] = 
                    ((left[i] << 8) | left[i + 1]); //Lepton_Frame에 픽셀 위치 맞게 2 바이트(1 픽셀) 저장
            }

            // 위와 동일한 구문
            for (int i = 4; i < PACKET_SIZE; i += 2)
            {
                int col = 80 + (i - 4) / 2; // 홀수 pk_num은 80행 부터 시작
                Lepton_Frame[row][col] =
                    ((right[i] << 8) | right[i + 1]);
            }

            row++; // 두 패킷 합쳐서 한 줄 완성
        }
    }

    frame_ready = 1; // Lepton_Frame에 모든 픽셀 저장 시 flag set
}

// ProcessFrame 으로 만든 16bit 프레임을 8bit 3c채널로 정규화 시키고 Normalized_Frame으로 전달
void Normalize(uint16_t Lepton_Frame[FRAME_HEIGHT][FRAME_WIDTH], 
                uint8_t Normalized_Frame[FRAME_HEIGHT][FRAME_WIDTH][3])
{
    // OpenCV에게 이 주소에 160×120 크기의 16비트 1채널 이라고 알려주기
    cv::Mat frame16(FRAME_HEIGHT, FRAME_WIDTH, CV_16UC1, (void*)Lepton_Frame);
    
    // 한 프레임 내 픽셀 값을 전부 확인해서 min, max 값을 찾아 저장
    double minVal, maxVal;
    cv::minMaxLoc(frame16, &minVal, &maxVal);

    // Frame의 모든 픽셀이 같은 상황(하드웨어 error)에서 0으로 나누는 상황을 방지
    if (minVal == maxVal) maxVal = minVal + 1;

    // 16비트를 8비트로 정규화 하는 과정 
    cv::Mat frame8;
    frame16.convertTo(frame8, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));

    // 8비트로 정규화된 프레임을 8x8 = 64블록으로 나눠서 적당한 대비(2.0)로 조절  
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(2.0);
    clahe->setTilesGridSize(cv::Size(8, 8));

    cv::Mat claheFrame;
    clahe->apply(frame8, claheFrame); // 적용해서 claheFrame 으로
    
    // 3채널로 바꾸는 과정 (yolo 모델은 입력이 3채널)
    cv::Mat frame3ch;
    cv::cvtColor(claheFrame, frame3ch, cv::COLOR_GRAY2BGR);

    memcpy(Normalized_Frame, frame3ch.data, FRAME_HEIGHT * FRAME_WIDTH * 3 * sizeof(uint8_t));

    // AI가 객체를 더 잘 판단하게 하기 위해 프레임을 정규화 하는 과정

}


// 공유 버퍼에 Lepton 패킷 쓰는 함수
void *ReadThread(void *arg)
{
    uint8_t packet[PACKET_SIZE]; // 1패킷 저장할 공간
    int ret; 

    while (1)
    {
        ret = SpiReadPacket(packet); // 읽기

        if (ret == 0)
        {
            PacketBuffer_Push(&PacketBuffer, packet); //성공 시 공유 버퍼에 Push
        }
        else
        {
            usleep(1000); // 실패시 쉬었다 다시
        }
    }
    return NULL;
}

// 공유 버퍼의 패킷을 꺼내와서 해석하는 함수
void *ProcessThread(void *arg)
{   
    uint8_t packet[PACKET_SIZE];
    uint8_t segment_buf[4][60][PACKET_SIZE];
    static uint8_t Normalized_Frame[FRAME_HEIGHT][FRAME_WIDTH][3];
    static int Reset_count1 = 0; // 버림 패킷 스킵 횟수 제한
    static int Reset_count2 = 0; // pk_num 불일치 횟수 제한
    int expected_pk = 0; // 다음 pk_num 
    int expected_seg = 1; // 다음 seg_id 
    int synced = 0; // 동기화 여부 , 동기화는 프레임의 시작지점을 찾았는지 여부

    while (1)
    {
        // 버퍼에서 패킷 하나 꺼냄
        PacketBuffer_Pop(&PacketBuffer, packet);

        // 버림 패킷 확인 
        if ((packet[0] & 0x0F) == 0x0F) // 데이터 시트 상 xFxx 는 버림 패킷 명시
        {
            Reset_count1++;
            if (Reset_count1 > 1400) // 버림 패킷 1400개 이상이면 Lepton Reset
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

        // pk_num 추출 (packet[1]) -> 패킷의 두번째 바이트는 pk_num
        uint8_t pk_num = packet[1]; 

        // 아직 동기화 안 된 상태면 첫 시작 패킷 탐색
        if (!synced)
        {
            if (pk_num == 0) // 항상 프레임의 첫 시작은 seg_id = 1 , pk_num =0 인 지점
            {
                synced = 1;
                expected_seg = 1;
                expected_pk = 0;
            }
            else
                continue;
        }

        // 예상 패킷 번호 불일치 → 동기화 해제 후 재탐색 , 정상적인 동작에서 패킷 번호는 1씩 순차적으로 증가함을 기대함
        if (pk_num != expected_pk)
        {   
            Reset_count2++;
            synced = 0;
            expected_pk = 0;
            expected_seg = 1;

            if (Reset_count2 > 500) // 패킷 번호가 1씩 순차적으로 증가하는 패턴 500번 반복 동안 찾지 못한다면 Reset
            { 
                Lepton_Reset();     
                Reset_count2 = 0;
                usleep(1000000);      
            }
            continue;
               
        }
        

        // pk_num == 20일 때만 seg_id 검사 -> 데이터 시트에서 pk_num 20 에 seg_id 가 적혀있다고 명시 
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

        // 정상 패킷 저장
        memcpy(segment_buf[expected_seg - 1][pk_num], packet, PACKET_SIZE);
        expected_pk++;

        // 세그먼트 완성 → 다음 세그먼트로 , 세그먼트도 순차적으로 1씩 증가한다고 기대함
        if (expected_pk >= 60)
        {
            expected_pk = 0;
            expected_seg++;
        }

        // 세그먼트 완성 → 프레임 조립
        if (expected_seg > 4)
        {
            ProcessFrame(segment_buf); // 프레임 만들기

            Normalize(Lepton_Frame, Normalized_Frame); // 프레임을 정규화 시키기

            TCP_Lepton_Buffer_Push(Normalized_Frame); // AI 폴더에 전달하기 위함. 자세한 설명은 AI_Buffer.cpp 에 있음

            expected_seg = 1;
            expected_pk = 0;
            synced = 0;
            Reset_count1 = 0;
            Reset_count2 = 0;
            
        }
    }

    return NULL;
}



