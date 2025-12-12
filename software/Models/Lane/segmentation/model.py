import torch
import torch.nn as nn
import torch.nn.functional as F


class DoubleConv(nn.Module):
    """
    Conv → BN → ReLU 두 번
    in_channels  → out_channels
    """
    def __init__(self, in_channels: int, out_channels: int):
        super().__init__()
        self.block = nn.Sequential(
            nn.Conv2d(in_channels, out_channels, kernel_size=3, padding=1, bias=False),
            nn.BatchNorm2d(out_channels),
            nn.ReLU(inplace=True),

            nn.Conv2d(out_channels, out_channels, kernel_size=3, padding=1, bias=False),
            nn.BatchNorm2d(out_channels),
            nn.ReLU(inplace=True),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.block(x)


class Down(nn.Module):
    """
    Down-sampling block:
    MaxPool(2) → DoubleConv
    """
    def __init__(self, in_channels: int, out_channels: int):
        super().__init__()
        self.pool = nn.MaxPool2d(2)
        self.conv = DoubleConv(in_channels, out_channels)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        x = self.pool(x)
        x = self.conv(x)
        return x


class Up(nn.Module):
    """
    Up-sampling block:
    Upsample(2x, bilinear) → concat(skip) → DoubleConv
    """
    def __init__(self, in_channels: int, out_channels: int):
        """
        in_channels: upsample된 feature + skip feature 가 concat 된 채널 수
        out_channels: 최종 출력 채널 수
        """
        super().__init__()
        # bilinear 업샘플
        self.up = nn.Upsample(scale_factor=2, mode="bilinear", align_corners=False)
        self.conv = DoubleConv(in_channels, out_channels)

    def forward(self, x: torch.Tensor, skip: torch.Tensor) -> torch.Tensor:
        # 1) 업샘플
        x = self.up(x)

        # 2) 크기 안 맞으면 패딩으로 맞춰주기 (안전장치)
        diff_y = skip.size(2) - x.size(2)
        diff_x = skip.size(3) - x.size(3)

        if diff_y != 0 or diff_x != 0:
            x = F.pad(
                x,
                [diff_x // 2, diff_x - diff_x // 2,
                 diff_y // 2, diff_y - diff_y // 2]
            )

        # 3) channel 방향으로 concat
        x = torch.cat([skip, x], dim=1)

        # 4) DoubleConv
        x = self.conv(x)
        return x


class OutConv(nn.Module):
    """
    마지막 1x1 conv
    out_channels = num_classes (binary일 땐 1)
    """
    def __init__(self, in_channels: int, out_channels: int):
        super().__init__()
        self.conv = nn.Conv2d(in_channels, out_channels, kernel_size=1)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.conv(x)


class UNet(nn.Module):
    """
    4단 UNet
    입력: (B, 3, 640, 640)
    출력: (B, 1, 640, 640)  ← 로짓 (sigmoid는 나중에)
    """
    def __init__(self, in_channels: int = 3, num_classes: int = 1, base_c: int = 32):
        super().__init__()

        # Encoder
        self.inc = DoubleConv(in_channels, base_c)          # 3  → 32
        self.down1 = Down(base_c, base_c * 2)               # 32 → 64
        self.down2 = Down(base_c * 2, base_c * 4)           # 64 → 128
        self.down3 = Down(base_c * 4, base_c * 8)           # 128 → 256

        # Bottleneck
        self.bridge = DoubleConv(base_c * 8, base_c * 16)   # 256 → 512

        # Decoder
        self.up3 = Up(base_c * 16 + base_c * 8, base_c * 8) # (512+256) → 256
        self.up2 = Up(base_c * 8 + base_c * 4, base_c * 4)  # (256+128) → 128
        self.up1 = Up(base_c * 4 + base_c * 2, base_c * 2)  # (128+64)  → 64
        self.up0 = Up(base_c * 2 + base_c, base_c)          # (64+32)   → 32

        # Output
        self.outc = OutConv(base_c, num_classes)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        # Encoder
        x0 = self.inc(x)     # (B, 32, 640, 640)
        x1 = self.down1(x0)  # (B, 64, 320, 320)
        x2 = self.down2(x1)  # (B, 128,160, 160)
        x3 = self.down3(x2)  # (B, 256, 80, 80)

        # Bottleneck
        xb = self.bridge(x3) # (B, 512, 40, 40)

        # Decoder (skip 연결)
        x = self.up3(xb, x3) # (B, 256, 80, 80)
        x = self.up2(x, x2)  # (B, 128,160,160)
        x = self.up1(x, x1)  # (B, 64, 320,320)
        x = self.up0(x, x0)  # (B, 32, 640,640)

        logits = self.outc(x)  # (B, 1, 640, 640)
        return logits


if __name__ == "__main__":
    # 간단한 동작 테스트
    model = UNet(in_channels=3, num_classes=1, base_c=32)
    x = torch.randn(1, 3, 640, 640)
    y = model(x)
    print("입력:", x.shape)
    print("출력:", y.shape)
