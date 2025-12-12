import struct


LEPTON_FRAME = 1
RGB_FRAME    = 2
PACKET_TYPE_LANE   = 3
PACKET_TYPE_OBJECT = 4   


FRAME_HEADER_FORMAT = "<5I"   
FRAME_HEADER_SIZE  = struct.calcsize(FRAME_HEADER_FORMAT)


def pack_frame_header(frame_type, width, height, channels, data_size):
    return struct.pack(
        FRAME_HEADER_FORMAT, 
        frame_type, 
        width, 
        height, 
        channels, 
        data_size
    )


def unpack_frame_header(header_bytes):
    return struct.unpack(FRAME_HEADER_FORMAT, header_bytes)



# =========================================================
#   LANE PACKET
# =========================================================
def make_lane_packet(lane_points):

    fixed = []

    for (x, y) in lane_points:
        # None / 음수 / 65535 초과 → 에러 방지
        if x is None or y is None:
            continue

        x = max(0, min(65535, int(x)))
        y = max(0, min(65535, int(y)))

        fixed.append((x, y))

    count = len(fixed)

    # HEADER (8 bytes, 기존 구조 그대로 유지)
    header = struct.pack(
        "<II",
        PACKET_TYPE_LANE,
        count
    )

    payload = b""
    for (x, y) in fixed:
        payload += struct.pack("<HH", x, y)

    return header + payload



# =========================================================
#   OBJECT PACKET
# =========================================================
def make_object_packet(objects):

    count = len(objects)

    # HEADER (8 bytes)
    header = struct.pack(
        "<II",
        PACKET_TYPE_OBJECT,
        count
    )

    payload = b""

    for (cls, conf, x1, y1, x2, y2) in objects:

        conf_i = int(conf * 1000)  # float → int 변환
        
        payload += struct.pack(
            "<HHHHHH",
            int(cls),
            conf_i,
            int(x1),
            int(y1),
            int(x2),
            int(y2)
        )

    return header + payload
