import os
import time
import torch
import torch.nn as nn
from torch.utils.data import DataLoader
import datetime

from dataset import LaneSegmentationDataset
from model import UNet

# ===============================
# 설정값
# ===============================
DATA_ROOT = "dataset/letterbox_dataset"

BATCH_SIZE = 4
NUM_EPOCHS = 28
LEARNING_RATE = 1e-4
NUM_WORKERS = 4

SAVE_DIR = "segmentation/checkpoints"
RESUME = True

os.makedirs(SAVE_DIR, exist_ok=True)
torch.backends.cudnn.benchmark = True

BEST_IOU_TXT = os.path.join(SAVE_DIR, "best_iou.txt")
EPOCH_FILE = os.path.join(SAVE_DIR, "last_epoch.txt")

# ===============================
# Metric
# ===============================
def compute_iou(preds, targets, eps=1e-6):
    intersection = (preds * targets).sum(dim=(1,2,3))
    union = preds.sum(dim=(1,2,3)) + targets.sum(dim=(1,2,3)) - intersection
    return ((intersection + eps) / (union + eps)).mean().item()

def compute_dice(preds, targets, eps=1e-6):
    intersection = (preds * targets).sum(dim=(1,2,3))
    sums = preds.sum(dim=(1,2,3)) + targets.sum(dim=(1,2,3))
    return ((2 * intersection + eps) / (sums + eps)).mean().item()

# ===============================
# Train
# ===============================
def train():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"🔥 Device: {device}")

    train_dataset = LaneSegmentationDataset(DATA_ROOT, split="train")
    val_dataset = LaneSegmentationDataset(DATA_ROOT, split="val")

    train_loader = DataLoader(
        train_dataset,
        batch_size=BATCH_SIZE,
        shuffle=True,
        num_workers=NUM_WORKERS,
        pin_memory=True
    )

    val_loader = DataLoader(
        val_dataset,
        batch_size=BATCH_SIZE,
        shuffle=False,
        num_workers=NUM_WORKERS,
        pin_memory=True
    )

    model = UNet(in_channels=3, num_classes=1, base_c=32).to(device)
    criterion = nn.BCEWithLogitsLoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=LEARNING_RATE)

    start_epoch = 1
    best_val_iou = 0.0

    # ===============================
    # ✅ 이전 best IoU 불러오기
    # ===============================
    if os.path.exists(BEST_IOU_TXT):
        with open(BEST_IOU_TXT, "r") as f:
            best_val_iou = float(f.read().strip())
        print(f"🔹 이전 Best IoU 불러옴: {best_val_iou:.4f}")

    # ===============================
    # ✅ 이전 epoch 불러오기
    # ===============================
    if os.path.exists(EPOCH_FILE):
        with open(EPOCH_FILE, "r") as f:
            last_epoch_done = int(f.read().strip())
            start_epoch = last_epoch_done + 1
        print(f"🔄 이전 학습 epoch: {last_epoch_done} → {start_epoch}부터 재개")

    # ===============================
    # ✅ 모델 이어서 불러오기
    # ===============================
    last_model_path = os.path.join(SAVE_DIR, "last_unet.pth")

    if RESUME and os.path.exists(last_model_path):
        print("🔁 저장된 모델 가중치 불러오는 중...")
        model.load_state_dict(torch.load(last_model_path, map_location=device))
        print("✅ 모델 복원 완료")

    # ===============================
    # Training Loop
    # ===============================
    for epoch in range(start_epoch, NUM_EPOCHS + 1):
        start = time.time()

        model.train()
        train_loss = 0.0

        for images, labels in train_loader:
            images = images.to(device)
            labels = labels.to(device)

            logits = model(images)
            loss = criterion(logits, labels)

            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

            train_loss += loss.item()

        avg_train_loss = train_loss / len(train_loader)

        # ===============================
        # Validation
        # ===============================
        model.eval()
        val_loss = 0.0
        val_iou = 0.0
        val_dice = 0.0

        with torch.no_grad():
            for images, labels in val_loader:
                images = images.to(device)
                labels = labels.to(device)

                logits = model(images)
                loss = criterion(logits, labels)

                probs = torch.sigmoid(logits)
                preds = (probs > 0.5).float()

                val_loss += loss.item()
                val_iou += compute_iou(preds, labels)
                val_dice += compute_dice(preds, labels)

        avg_val_loss = val_loss / len(val_loader)
        avg_val_iou = val_iou / len(val_loader)
        avg_val_dice = val_dice / len(val_loader)

        elapsed = time.time() - start

        print(f"\n[Epoch {epoch}/{NUM_EPOCHS}] - {elapsed:.1f}s")
        print(f" Train Loss : {avg_train_loss:.4f}")
        print(f" Val Loss   : {avg_val_loss:.4f}")
        print(f" Val IoU    : {avg_val_iou:.4f}")
        print(f" Val Dice   : {avg_val_dice:.4f}")

        # ===============================
        # ✅ 항상 last 모델 저장
        # ===============================
        torch.save(model.state_dict(), os.path.join(SAVE_DIR, "last_unet.pth"))

        # ===============================
        # ✅ 현재 epoch 기록
        # ===============================
        with open(EPOCH_FILE, "w") as f:
            f.write(str(epoch))

        # ===============================
        # ✅ Best 모델 갱신 + 자동 백업
        # ===============================
        if avg_val_iou > best_val_iou:
            best_val_iou = avg_val_iou

            # 기본 best
            torch.save(model.state_dict(), os.path.join(SAVE_DIR, "best_unet.pth"))

            # 시간 기반 백업
            timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_name = f"best_unet_epoch{epoch}_IoU{avg_val_iou:.4f}_{timestamp}.pth"
            backup_path = os.path.join(SAVE_DIR, backup_name)

            torch.save(model.state_dict(), backup_path)

            with open(BEST_IOU_TXT, "w") as f:
                f.write(str(best_val_iou))

            print(f"✅ Best 갱신: IoU = {best_val_iou:.4f}")
            print(f"📦 백업 저장됨: {backup_name}")

    print("\n✅ 학습 완료")
    print(f"🔥 최종 Best IoU : {best_val_iou:.4f}")


if __name__ == "__main__":
    train()
