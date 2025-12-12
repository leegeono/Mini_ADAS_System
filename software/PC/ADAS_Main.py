import socket
import numpy as np
import sys, os
import msvcrt   # 키 입력 감지

# ============================================
# 경로 설정
# ============================================
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(CURRENT_DIR, ".."))
sys.path.append(ROOT)

# ============================================
# AI modules (🔥 수정된 lane_model_1 사용)
# ============================================
from PC.AI.inference_lane import run_lane_model       
from PC.AI.inference_yolo import run_yolo_RGB_model, run_yolo_Lepton_model

# ============================================
# TCP modules
# ============================================
from PC.TCP_PC.TCP_Utils import recv_frame, send_packet
from PC.TCP_PC.TCP_Protocol import (
    LEPTON_FRAME,
    RGB_FRAME,
    make_lane_packet,
    make_object_packet
)

# ============================================
# 차선 L,R를 패킷용 좌표 리스트로 변환
# ============================================
def lane_to_packet_points(L,R):
    pts=[]
    for x,y in L+R:
        if x is not None:
            pts.append((int(x),int(y)))
    return pts


# ============================================
# 설정
# ============================================
SERVER_IP = "0.0.0.0"
SERVER_PORT = 5000
MODE = 1


# ============================================
# ADAS MAIN LOOP
# ============================================
def main():
    global MODE

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((SERVER_IP, SERVER_PORT))
    server.listen(1)

    print("[ADAS] Raspberry Pi 연결 대기...")
    conn, addr = server.accept()
    print("[ADAS] 연결됨:", addr)

    while True:

        # ---------------- KEY INPUT ----------------
        if msvcrt.kbhit():
            key = msvcrt.getch().decode()
            if key=="1":
                MODE=1; print("📢 MODE 1 → RGB ADAS (Lane+YOLO)")
            elif key=="2":
                MODE=2; print("📢 MODE 2 → Split RGB/Lepton")


        # ---------------- FRAME RX ----------------
        frame_type,frame = recv_frame(conn)
        if frame is None:
            break


        # 🔥 MODE 1 = RGB Lane + Object YOLO
        if MODE==1 and frame_type==RGB_FRAME:

            L,R,roi,mask = run_lane_model(frame)              # ← lane inference
            lane_pts = lane_to_packet_points(L,R)              # ← 패킷화
            send_packet(conn, make_lane_packet(lane_pts))       # lane TX
            send_packet(conn, make_object_packet(run_yolo_RGB_model(frame)))  # yolo TX


        # 🔥 MODE 2 = (RGB=Lane만) + (Lepton=YOLO)
        elif MODE==2:
            if frame_type==RGB_FRAME:
                L,R,roi,mask = run_lane_model(frame)
                send_packet(conn, make_lane_packet(lane_to_packet_points(L,R)))
            elif frame_type==LEPTON_FRAME:
                send_packet(conn, make_object_packet(run_yolo_Lepton_model(frame)))


    conn.close()
    server.close()



if __name__ == "__main__":
    main()
