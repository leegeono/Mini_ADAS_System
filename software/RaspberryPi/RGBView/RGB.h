#ifndef RGB_H
#define RGB_H

#include <stdint.h>
#include <pthread.h>

// RGB 카메라 스레드 함수
void* RGB_Thread(void* arg);

#endif