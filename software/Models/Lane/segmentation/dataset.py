import os
import cv2
import torch
from torch.utils.data import Dataset
import numpy as np


class LaneSegmentationDataset(Dataset):
    """
    letterbox_dataset용 PyTorch Dataset
    이미지: 640×640 RGB
    라벨:   640×640 binary mask
    """

    def __init__(self, root_dir, split="train"):
        """
        root_dir: dataset/letterbox_dataset
        split: train / val
        """
        self.img_dir = os.path.join(root_dir, "images", split)
        self.lab_dir = os.path.join(root_dir, "labels", split)

        self.img_files = sorted(
            [f for f in os.listdir(self.img_dir) if f.endswith(".jpg")]
        )

        assert len(self.img_files) > 0, "이미지 파일이 없습니다!"

    def __len__(self):
        return len(self.img_files)

    def __getitem__(self, idx):
        img_name = self.img_files[idx]

        img_path = os.path.join(self.img_dir, img_name)
        lab_path = os.path.join(self.lab_dir, img_name.replace(".jpg", ".png"))

        # 이미지 읽기
        image = cv2.imread(img_path)
        image = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)

        # 라벨 읽기
        label = cv2.imread(lab_path, cv2.IMREAD_GRAYSCALE)

        if image is None or label is None:
            raise ValueError(f"파일 읽기 실패: {img_name}")

        # 0~255 -> 0~1 정규화
        image = image.astype(np.float32) / 255.0
        label = (label > 0).astype(np.float32)

        # HWC -> CHW 변환
        image = np.transpose(image, (2, 0, 1))
        label = np.expand_dims(label, axis=0)

        image = torch.tensor(image, dtype=torch.float32)
        label = torch.tensor(label, dtype=torch.float32)

        return image, label


if __name__ == "__main__":
    # 간단 테스트
    dataset = LaneSegmentationDataset("dataset/letterbox_dataset", split="train")

    print("✅ 전체 데이터 개수:", len(dataset))

    img, lab = dataset[0]

    print("이미지 shape:", img.shape)   # (3, 640, 640)
    print("라벨 shape:", lab.shape)     # (1, 640, 640)
    print("이미지 min/max:", img.min().item(), img.max().item())
    print("라벨 unique:", torch.unique(lab))
