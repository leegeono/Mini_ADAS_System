#include "Lane3D.h"
#include "Object3D.h"

#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>


static inline float QuantizeLane3(float x)
{
    if (x < -1.5f) return -4.5f;  // 왼
    if (x >  1.5f) return  4.5f;  // 오른
    return 0.0f;                  // 중앙
}

// ================================================
// 도로 설정
// ================================================
static const float ROAD_LENGTH = 100.0f;
static const float ROAD_HALF_WIDTH = 8.0f;

// ================================================
// 내부 버퍼
// ================================================
static std::vector<Lane> laneBuf;
static std::vector<Object> objectBuf;

// ================================================
// 카메라 상태
// ================================================
static float camYaw   = 0.0f;
static float camPitch = -10.0f;

// 줌 거리 (휠 변경)
static float camDist = 30.0f;
static const float camMin = 5.0f;
static const float camMax = 80.0f;

static int lastX = 0;
static int lastY = 0;
static bool mouseDown = false;

// ================================================
// 도로 그리기
// ================================================
static void DrawRoad()
{
    glColor3f(0.20f, 0.20f, 0.20f);

    glBegin(GL_QUADS);
    glVertex3f(-ROAD_HALF_WIDTH, 0, 0);
    glVertex3f( ROAD_HALF_WIDTH, 0, 0);
    glVertex3f( ROAD_HALF_WIDTH, 0, ROAD_LENGTH);
    glVertex3f(-ROAD_HALF_WIDTH, 0, ROAD_LENGTH);
    glEnd();
}

// ======================================================
// Catmull-Rom Spline 기반 스무스 곡선 생성
// ======================================================
static std::vector<Lane> SplineSmooth(const std::vector<Lane>& P)
{
    std::vector<Lane> out;
    int n = P.size();
    if (n < 4) return P;

    for (int i = 1; i < n - 2; i++)
    {
        Lane p0 = P[i - 1];
        Lane p1 = P[i];
        Lane p2 = P[i + 1];
        Lane p3 = P[i + 2];

        for (float t = 0; t <= 1.0f; t += 0.03f)
        {
            float t2 = t * t;
            float t3 = t2 * t;

            float fx = 0.5f *
                ((2 * p1.x) +
                 (-p0.x + p2.x) * t +
                 (2*p0.x - 5*p1.x + 4*p2.x - p3.x) * t2 +
                 (-p0.x + 3*p1.x - 3*p2.x + p3.x) * t3);

            float fy = 0.5f *
                ((2 * p1.y) +
                 (-p0.y + p2.y) * t +
                 (2*p0.y - 5*p1.y + 4*p2.y - p3.y) * t2 +
                 (-p0.y + 3*p1.y - 3*p2.y + p3.y) * t3);

            out.push_back({ (uint16_t)fx, (uint16_t)fy });
        }
    }
    return out;
}

// ======================================================
// 부드러운 평균 X smoothing
// ======================================================
static float smoothLX = 320.0f;
static float smoothRX = 320.0f;

static inline float smoothUpdate(float prev, float target)
{
    float alpha = 0.25f;
    return prev * (1 - alpha) + target * alpha;
}

// ======================================================
// 차선 그리기
// ======================================================
static void DrawLane()
{
    if (laneBuf.empty()) return;

    // =============================================================
    // 🔥 0) X 양자화 적용 (울렁거림 감소 핵심)
    // =============================================================
    std::vector<Lane> qbuf;
    qbuf.reserve(laneBuf.size());

    for (auto& p : laneBuf)
    {
        // y 비례해 bucket 크기 증가 (멀리 갈수록 더 작은 흔들림)
        float t = p.y / 640.0f;
        int bucket = (int)round(4 + 6 * t);  // 🔥 가까울수록 4픽셀, 멀리 10픽셀 단위

        int qx = (p.x / bucket) * bucket;    // 🔥 양자화된 X
        qbuf.push_back({ (uint16_t)qx, p.y });
    }

    // =============================================================
    // 1) 좌/우 차선 분류
    // =============================================================
    std::vector<Lane> L, R;

    for (auto &p : qbuf)   // 🔥 기존 laneBuf → qbuf 로 교체됨
        (p.x < 320 ? L : R).push_back(p);

    auto calcAvgX = [&](std::vector<Lane>& V){
        if (V.empty()) return NAN;
        float s = 0;
        for (auto &p : V) s += p.x;
        return s / V.size();
    };

    float L_px = calcAvgX(L);
    float R_px = calcAvgX(R);

    // =============================================================
    // 2) px → 3D X 변환
    // =============================================================
    auto to3DX = [&](float px){
        float nx = (px - 320.0f) / 320.0f;
        return nx * ROAD_HALF_WIDTH;
    };

    float LX = std::isnan(L_px) ? NAN : to3DX(L_px);
    float RX = std::isnan(R_px) ? NAN : to3DX(R_px);

    // =============================================================
    // 3) Adaptive Smoothing
    // =============================================================
    static float smoothLX = 0;
    static float smoothRX = 0;
    static bool initialized = false;

    auto smooth = [&](float prev, float target)
    {
        float diff = fabs(prev - target);
        float alpha = 0.02f + std::min(diff * 0.0010f, 0.04f);
        return prev * (1.0f - alpha) + target * alpha;
    };

    if (!initialized)
    {
        if (!std::isnan(LX)) smoothLX = LX;
        if (!std::isnan(RX)) smoothRX = RX;
        initialized = true;
    }

    if (!std::isnan(LX)) smoothLX = smooth(smoothLX, LX);
    if (!std::isnan(RX)) smoothRX = smooth(smoothRX, RX);

    // =============================================================
    // 4) 한쪽만 있을 때 자동 생성
    // =============================================================
    const float LANE_WIDTH = ROAD_HALF_WIDTH * 0.5f;

    bool L_exist = !std::isnan(LX);
    bool R_exist = !std::isnan(RX);

    if (L_exist && !R_exist)       smoothRX = smoothLX + LANE_WIDTH;
    else if (!L_exist && R_exist)  smoothLX = smoothRX - LANE_WIDTH;
    else if (!L_exist && !R_exist) return;

    // =============================================================
    // 5) 차선 렌더링 (하얀색 얇은 선)
    // =============================================================
    auto drawLineX = [&](float X)
    {
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(3);

        glBegin(GL_LINES);
        glVertex3f(X, 0.01f, 0);
        glVertex3f(X, 0.01f, ROAD_LENGTH);
        glEnd();
    };

    drawLineX(smoothLX);
    drawLineX(smoothRX);
}

// ======================================================
// 객체 그리기
// ======================================================
static void DrawObject(const std::vector<Object>& objs)
{
    if (objs.empty()) return;

    const float egoScale = 1.5f;
    const float egoZ = 5.0f;
    const float MAX_Z = 40.0f;

    const float MIN_W = 30.0f;
    const float MIN_H = 30.0f;

    const float X_SCALE = 1.8f;

    // ----------------------------------------------
    // 🔥 자동 ROI 계산 (프레임 높이의 58% 지점)
    // ----------------------------------------------
    const float FRAME_H = 640.0f;    // YOLO 입력 기준
    const float ROI_Y = FRAME_H * 0.58f;   // = 371.2

    for (auto& obj : objs)
    {
        // ----------------------------------------------
        // 🔥 ROI 위쪽(도로나 객체와 무관) → 스킵
        // ----------------------------------------------
        if (obj.y2 < ROI_Y)
            continue;

        float w = obj.x2 - obj.x1;
        float h = obj.y2 - obj.y1;

        bool isPerson = (obj.cls == 0);

        if (!isPerson)
        {
            if (w < MIN_W || h < MIN_H)
                continue;
        }

        float cx = (obj.x1 + obj.x2) * 0.5f;
        float cy = (obj.y1 + obj.y2) * 0.5f;

        float Z = (900.0f / h);
        if (Z < 3.0f) Z = 3.0f;
        if (Z > MAX_Z) Z = MAX_Z;
        Z += egoZ;

        float nx = (cx - 320.0f) / 320.0f;
        float perspective = (Z / (Z + 20.0f));
        float X = -nx * ROAD_HALF_WIDTH * perspective * X_SCALE;

        if (!isPerson)
            X = QuantizeLane3(X);

        if (isPerson)
        {
            if (X > 0) X += 3.5f;
            else       X -= 3.5f;
        }

        float scale = egoScale;

        if (isPerson)
            Object3D_DrawPerson(X, Z, scale);
        else
            Object3D_DrawCar(X, Z, scale);
    }
}

// ======================================================
// 마우스 버튼
// ======================================================
void mouse(int button, int state, int x, int y)
{
    // ---- 휠 처리 ----
    if (button == 3)  // wheel up
    {
        camDist -= 2.0f;
        if (camDist < camMin) camDist = camMin;
        glutPostRedisplay();
        return;
    }
    if (button == 4)  // wheel down
    {
        camDist += 2.0f;
        if (camDist > camMax) camDist = camMax;
        glutPostRedisplay();
        return;
    }

    // ---- 좌클릭 회전 ----
    if (button == GLUT_LEFT_BUTTON)
    {
        if (state == GLUT_DOWN)
        {
            mouseDown = true;
            lastX = x;
            lastY = y;
        }
        else
        {
            mouseDown = false;
        }
    }
}

// ======================================================
// 마우스 드래그 -> 회전
// ======================================================
void motion(int x, int y)
{
    if (!mouseDown) return;

    camYaw += (x - lastX) * 0.3f;
    camPitch += (y - lastY) * 0.3f;

    if (camPitch > 89) camPitch = 89;
    if (camPitch < -89) camPitch = -89;

    lastX = x;
    lastY = y;

    glutPostRedisplay();
}


// ======================================================
// Display
// ======================================================
static void display()
{
    glClearColor(0.88f, 0.88f, 0.90f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    float radYaw   = camYaw   * 3.14159f / 180.0f;
    float radPitch = camPitch * 3.14159f / 180.0f;

    float eyeX = camDist * cos(radPitch) * sin(radYaw);
    float eyeY = camDist * sin(radPitch) + 5.0f;
    float eyeZ = camDist * cos(radPitch) * cos(radYaw);

    gluLookAt(
    eyeX + 0.0f,         // ego X
    eyeY + 1.5f,         // ego 위쪽
    eyeZ - 10.0f,        // ego 뒤쪽

    0.0f, 0.5f, 20.0f,   // 바라보는 지점 = 차 앞 도로
    0.0f, 1.0f, 0.0f
    );

    DrawRoad();
    DrawLane();
    DrawObject(objectBuf);
    Object3D_DrawEgo();
    Object3D_SetEgo(0.0f,0.0f,1.5f);
    glutSwapBuffers();
}

// ======================================================
// Timer
// ======================================================
static void timer(int)
{
    std::vector<Lane> temp;
    if (TCP_LANE_Buffer_Pop(temp))
        laneBuf = temp;

    std::vector<Object> tempObj;
    if (TCP_Object_Buffer_Pop(tempObj))
        objectBuf = tempObj;

    glutPostRedisplay();
    glutTimerFunc(33, timer, 0);
}

// ======================================================
// API
// ======================================================
void Lane3D_InitOpenGL(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("ADAS 3D Viewer");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, 1024.0f / 768.0f, 0.1f, 2000.0f);

    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);

    Object3D_Init();

    glutDisplayFunc(display);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);;
    glutTimerFunc(0, timer, 0);
}

void Lane3D_StartLoop()
{
    glutMainLoop();
}
