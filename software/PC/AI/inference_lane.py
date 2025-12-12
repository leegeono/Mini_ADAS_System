import cv2, torch, numpy as np, os, sys
from torchvision import transforms

CURRENT_DIR=os.path.dirname(os.path.abspath(__file__))
ROOT=os.path.abspath(os.path.join(CURRENT_DIR,"..",".."))
sys.path.append(ROOT)

from Models.Lane.segmentation.model import UNet

device="cuda" if torch.cuda.is_available() else "cpu"
to_tensor=transforms.ToTensor()

model=UNet(in_channels=3,num_classes=1).to(device)
state=torch.load(os.path.join(ROOT,"Models","Lane","segmentation","checkpoints","best_unet.pth"),
                 map_location=device)
model.load_state_dict(state)
model.eval()


def fill_bottom_hybrid(points, split_ratio=0.35):
    pts=points[:]
    valid=[(x,y) for x,y in pts if x!=None]
    if len(valid)<4: return pts
    
    ys=np.array([y for x,y in valid])
    xs=np.array([x for x,y in valid])
    y_min,y_max=ys.min(),ys.max()
    split_y=y_min+(y_max-y_min)*split_ratio
    
    bottom=[(x,y) for x,y in valid if y>split_y]
    if len(bottom)<3: return pts
    
    ys_b=np.array([y for x,y in bottom])
    xs_b=np.array([x for x,y in bottom])
    m,b=np.polyfit(ys_b,xs_b,1)
    
    for i,(x,y) in enumerate(pts):
        if x is None and y>split_y:
            pts[i]=(int(m*y+b),y)
    return pts


def bottom_anchor(points,max_delta=40):
    new=[]; base=None
    for x,y in points:
        if x is None:
            new.append((None,y)); continue
        if base is None:
            base=x; new.append((x,y)); continue
        if abs(x-base)>max_delta:
            x=base
        base=x
        new.append((x,y))
    return new


def run_lane_model(frame):

    H,W,_=frame.shape
    top_ratio,bottom_ratio=0.45,0.70
    top_w,bottom_w=0.14,0.65
    step=2; thresh=0.8

    img=cv2.cvtColor(frame,cv2.COLOR_BGR2RGB)

    with torch.no_grad():
        pred=torch.sigmoid(model(to_tensor(img).unsqueeze(0).to(device)))[0,0].cpu().numpy()

    mask=(pred>thresh).astype(np.uint8)
    h,w=mask.shape


    roi=np.array([
        [int(w*(0.5-top_w)),int(h*top_ratio)],
        [int(w*(0.5+top_w)),int(h*top_ratio)],
        [int(w*(0.5+bottom_w/2)),int(h*bottom_ratio)],
        [int(w*(0.5-bottom_w/2)),int(h*bottom_ratio)]
    ])

    roi_mask=np.zeros_like(mask)
    cv2.fillPoly(roi_mask,[roi],1)
    masked=mask*roi_mask


    L=[]; R=[]
    L_bottom=R_bottom=None


    for y in range(int(h*bottom_ratio),int(h*top_ratio),-step):
        xs=np.where(masked[y]==1)[0]

        if len(xs)==0:
            L.append((None,y)); R.append((None,y)); continue
        
        if L_bottom is None or R_bottom is None:
            mid=(roi[0][0]+roi[1][0])//2
            left=xs[xs<mid]; right=xs[xs>mid]
            Lx=int(np.median(left)) if len(left)>0 else None
            Rx=int(np.median(right))if len(right)>0 else None
            L.append((Lx,y)); R.append((Rx,y))

            if Lx and Rx:
                L_bottom=Lx; R_bottom=Rx
            continue

        lane_w=R_bottom-L_bottom
        lane_center=(L_bottom+R_bottom)//2

        dist=(h*bottom_ratio - y) / ((h*bottom_ratio)-(h*top_ratio)+1e-6)
        dyn_mid=int(lane_center*(1-dist) + lane_center*0.98*dist)

        left = xs[xs < dyn_mid]
        right= xs[xs > dyn_mid]

        Lx=int(np.median(left)) if len(left)>0 else None
        Rx=int(np.median(right))if len(right)>0 else None
        
        L.append((Lx,y)); R.append((Rx,y))

    L=bottom_anchor(fill_bottom_hybrid(L))
    R=bottom_anchor(fill_bottom_hybrid(R))

    return L,R,roi,masked     # 🔥 유지!!
