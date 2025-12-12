import struct
import numpy as np
from PC.TCP_PC.TCP_Protocol import (
    FRAME_HEADER_SIZE,
    unpack_frame_header,
    RGB_FRAME,
    LEPTON_FRAME
)


# ============================================================
# 1) 정확한 크기만큼 받아오는 함수 (TCP 기본기)
# ============================================================
def Receive_all(sock, size):
    """
    TCP는 한 번 recv로 size만큼 오지 않기 때문에
    원하는 크기(size)만큼 다 받을 때까지 받는다.
    """
    data = b""
    while len(data) < size:
        packet = sock.recv(size - len(data))
        if not packet:
            return None   # 연결 종료
        data += packet
    return data


# ============================================================
# 2) 프레임 수신 함수 (RGB / LEPTON 자동 처리)
# ============================================================
def recv_frame(sock):
    """
    Raspberry → PC
    HEADER(20B) + IMAGE_DATA 수신
    반환: (frame_type, frame)
    frame_type = LEPTON_FRAME or RGB_FRAME
    """

    # 1) HEADER (20 bytes)
    header_bytes = Receive_all(sock, FRAME_HEADER_SIZE)
    if not header_bytes:
        return None, None

    frame_type, width, height, channels, data_size = unpack_frame_header(header_bytes)

    # 2) 데이터 수신
    frame_bytes = Receive_all(sock, data_size)
    if not frame_bytes:
        return None, None

    # 3) numpy 이미지로 변환
    frame = np.frombuffer(frame_bytes, dtype=np.uint8)
    if frame_type in (RGB_FRAME, LEPTON_FRAME):
        frame = frame.reshape((height, width, channels))

    return frame_type, frame


# ============================================================
# 3) 패킷 전송 함수 (PC → Raspberry)
# ============================================================
def send_packet(sock, packet):
    """
    lane_packet, object_packet 등을 그대로 TCP로 보내는 함수
    """
    try:
        sock.sendall(packet)
        return True
    except Exception as e:
        print("[TCP_UTILS] send_packet error:", e)
        return False
