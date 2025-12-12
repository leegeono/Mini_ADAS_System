import sys, os
from ultralytics import YOLO
import cv2

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(CURRENT_DIR, "..", ".."))
sys.path.append(ROOT)

ALLOWED_CLASSES = {0, 2}  # 0(person), 2(car) 등 필요 골라서

MODEL_PATH = os.path.join(ROOT, "Models", "Object", "yolov8s.pt")
yolo_model = YOLO(MODEL_PATH)

def map_L_to_RGB(px, py):
    nx = px / 160.0
    ny = py / 120.0

    X = 473.900687 \
        + -834.747250*nx + -1384.135836*ny \
        + 5411.068320*nx*nx + -4531.576769*nx*ny + 3653.960418*ny*ny \
        + -3401.159136*nx**3 + 1555.585270*nx*nx*ny + 1615.917991*nx*ny*ny + -1949.301750*ny**3

    Y = -164.294931 \
        + 1710.596813*nx + -129.310201*ny \
        + -1623.617733*nx*nx + -1878.908085*nx*ny + 1233.389240*ny*ny \
        + 87.906454*nx**3 + 1802.376106*nx*nx*ny + -234.945449*nx*ny*ny + -362.462158*ny**3
    
    return int(X), int(Y)

def run_yolo_RGB_model(frame):
    global yolo_model

    results = yolo_model.predict(frame, verbose=False)[0]
    boxes = results.boxes

    objects = []  # (cls, conf, x1, y1, x2, y2) 

    for box in boxes:
        cls = int(box.cls[0])
        if cls not in ALLOWED_CLASSES:
            continue
        
        x1, y1, x2, y2 = box.xyxy[0].tolist()
        conf = float(box.conf[0].item())
        objects.append((cls, conf, x1, y1, x2, y2))

    return objects

def run_yolo_Lepton_model(frame):
    results = yolo_model.predict(frame, verbose=False)[0]
    objects = []

    H,W = frame.shape[:2]        # ex. 120x160 (Lepton)
    RGB_W, RGB_H = 640,480       # PC render 기준

    scale_x = RGB_W/W
    scale_y = RGB_H/H

    for box in results.boxes:
        cls = int(box.cls[0])
        if cls not in ALLOWED_CLASSES:
            continue

        x1,y1,x2,y2 = box.xyxy[0].tolist()
        conf = float(box.conf[0].item())

        # 🔥 Lepton -> RGB 정규화
        rx1 = int(x1 * scale_x)
        ry1 = int(y1 * scale_y)
        rx2 = int(x2 * scale_x)
        ry2 = int(y2 * scale_y)

        # 안전벨트 (튄값 방지)
        rx1,ry1 = max(0,min(65535,rx1)), max(0,min(65535,ry1))
        rx2,ry2 = max(0,min(65535,rx2)), max(0,min(65535,ry2))

        objects.append((cls, conf, rx1,ry1,rx2,ry2))

    return objects