#ifndef LeptonSPI_H
#define LeptonSPI_H


/* ===== SPI 통신용 헤더 ===== */
#include <stdio.h> 
#include <stdint.h> 
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <linux/spi/spidev.h>
/* =========================== */


/* ===== Lepton 관련 정의 ===== */
#define PACKET_SIZE 164
#define MAX_RETRY   10
/* =========================== */


/* ====== SPI 전역 변수 ====== */
extern int spi_cs0_fd;
extern unsigned char spi_mode;
extern unsigned char spi_bits_per_word;
extern unsigned int spi_speed;
/* =========================== */


/* ===== SPI 관련 함수 선언부 ===== */
int SpiOpenPort(void);
int SpiClosePort(void);
int SpiReadPacket(uint8_t *buf);
/* ================================== */


#endif /* LeptonSPI_H*/
