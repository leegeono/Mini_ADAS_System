
#include "LeptonSPI.h"

/* =================================  Lepton SPI 통신에 필요한 함수들 ===================================== */


int spi_cs0_fd;                       // SPI 장치 파일 디스크립터 ("/dev/spidev0.0" 오픈 시 반환값)
unsigned char spi_mode = SPI_MODE_3;  // SPI 모드 설정 (Mode 3: CPOL=1, CPHA=1)
unsigned char spi_bits_per_word = 8;  // 전송 단위 비트 수 
unsigned int spi_speed = 20000000;    // SPI 클럭 속도 (20MHz, Lepton 최대 지원 범위)
 
/* ===== SPI 포트 열기(초기화) 함수 ===== */
int SpiOpenPort(void)
{
    // SPI 초기 변수
    int status_value =0;                       // ioctl()의 리턴값 저장 (0=성공, -1=실패)
    const char *device = "/dev/spidev0.0";     // 고정된 SPI 디바이스 경로 (CE0 전용)
                            

    // SPI 디바이스 열기 
     spi_cs0_fd = open(device, O_RDWR);   // SPI 장치를 읽기/쓰기 모드(O_RDWR)로 오픈 -> 성공시 변수에 고유 인덱스 저장
     if (spi_cs0_fd < 0)                  // 열기에 실패한 경우
     {                
        perror("SPI open");               // 커널이 반환한 에러 메시지 출력
        return -1;                        // 실패 시 함수 종료
     } 
    
   

    // SPI 모드 설정 
    status_value = ioctl(spi_cs0_fd, SPI_IOC_WR_MODE, &spi_mode); // SPI 모드(Mode 3) 적용
    if (status_value < 0) 
    {
        perror("SPI mode set"); // 실패 시 에러 출력
        close(spi_cs0_fd);      // 장치 닫고 종료
        return -1;
    }
    


    // 비트 전송 단위 설정 
    status_value = ioctl(spi_cs0_fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bits_per_word);
    if (status_value < 0)
    {
        perror("SPI bits_per_word set"); // 실패 시 에러 출력
        close(spi_cs0_fd);               // 장치 닫고 종료
        return -1;
    }
   


    //SPI 속도 설정 
    status_value = ioctl(spi_cs0_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
    if (status_value < 0) 
    {
        perror("SPI speed set"); // 실패 시 에러 출력
        close(spi_cs0_fd);       // 장치 닫고 종료
        return -1; 
    }
    


    return spi_cs0_fd; 
}
/* ================================= */



/* ===== SPI 포트 닫기 함수 ===== */
int SpiClosePort(void)
{
    int status_value = -1;

    if (spi_cs0_fd < 0) // 열려있는지 확인
    {
        fprintf(stderr, "[SPI] Device not opened or invalid file descriptor\n");
        return -1;
    }


    status_value = close(spi_cs0_fd); // 닫기 수행
    if (status_value < 0) 
    {
        perror("[SPI] Error - Could not close SPI device"); // 실패 시 에러 출력
        return -1; // 종료
    }

    spi_cs0_fd = -1; // fd 초기화 (이중 close 방지)

    return status_value; // 0이면 성공
}
/* ================================= */

/* ===== SPI 데이터 읽어오는 함수 ===== */
int SpiReadPacket(uint8_t *buf)
{
    if (spi_cs0_fd < 0) // 열려있는지 확인
    {
        fprintf(stderr, "[SPI] Device not opened\n");
        return -1;
    }

    int bytes_read = 0; 
    int ret = 0;
    int retry = 0;

    memset(buf, 0, PACKET_SIZE);  // 버퍼 초기화

    while (bytes_read < PACKET_SIZE) // 164 바이트 씩 읽기 -> Lepton은 한 패킷이 164bytes(Raw 14 ,Telemetry Mode OFF)
    {
        ret = read(spi_cs0_fd, buf + bytes_read, PACKET_SIZE - bytes_read); // 읽은 데이터 수 = ret 저장 -> 164바이트 씩 읽기 위한 확인용

        if (ret < 0)  // 읽기 실패
        {
            perror("[SPI] read failed");
            return -1;
        }
        else if (ret == 0) // 읽지 못함
        {
            usleep(1000); // 1ms
            if (++retry > MAX_RETRY) // MAX_RETRY 까지 수행
             {
                fprintf(stderr, "[SPI] Read timeout (no data)\n");
                return -1;
            }
            continue; // 재시도
        }

        bytes_read += ret;  // 누적해서 채움

    }

    /*
       // ======== [디버깅용 코드 시작] ========
    printf("[SPI DEBUG] First 8 bytes: ");
    for (int i = 0; i < 8; i++)
        printf("%02X ", buf[i]);
    printf("\n");
    // ======== [디버깅용 코드 끝] ========
    */
    return 0;
}
/* ================================= */

/* =================================================================================================== */




