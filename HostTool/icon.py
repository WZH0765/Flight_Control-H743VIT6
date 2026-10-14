from PIL import Image, ImageDraw
import math

SIZE = 256
img = Image.new('RGB', (SIZE, SIZE), (255, 255, 255))
draw = ImageDraw.Draw(img)

cx, cy = SIZE // 2, SIZE // 2
BLACK = (20, 20, 20)

arm_len = 82          # 中心到旋翼的距离
w_root  = 45          # 机臂根部宽度（宽）
w_tip   = 5         # 机臂末端宽度（窄）

# ---- 4 条梯形机臂 ----
for angle_deg in [45, 135, 225, 315]:
    a = math.radians(angle_deg)
    dx, dy = math.cos(a), math.sin(a)
    px, py = -dy, dx              # 垂直方向单位向量

    x2 = cx + arm_len * dx
    y2 = cy + arm_len * dy

    # 梯形四个顶点
    p1 = (cx + px * w_root/2, cy + py * w_root/2)   # 根部上
    p2 = (x2 + px * w_tip/2,  y2 + py * w_tip/2)    # 末端上
    p3 = (x2 - px * w_tip/2,  y2 - py * w_tip/2)    # 末端下
    p4 = (cx - px * w_root/2, cy - py * w_root/2)   # 根部下
    draw.polygon([p1, p2, p3, p4], fill=BLACK)

    # 末端圆头（接旋翼处圆滑）
    r_tip = w_tip // 2
    draw.ellipse((x2 - r_tip, y2 - r_tip,
                  x2 + r_tip, y2 + r_tip),
                 fill=BLACK)

# ---- 中心机身大圆（覆盖机臂根部，形成平滑过渡）----
body_r = 34
draw.ellipse((cx - body_r, cy - body_r, cx + body_r, cy + body_r),
             fill=BLACK)

# ---- 4 个旋翼（浅灰正圆）----
motor_r = 30
for angle_deg in [45, 135, 225, 315]:
    a = math.radians(angle_deg)
    mx = cx + arm_len * math.cos(a)
    my = cy + arm_len * math.sin(a)

    draw.ellipse((mx - motor_r, my - motor_r,
                  mx + motor_r, my + motor_r),
                 fill=(190, 190, 190))

    dot_r = 8
    draw.ellipse((mx - dot_r, my - dot_r, mx + dot_r, my + dot_r),
                 fill=BLACK)

# ---- 保存 ----
img.save('icon.ico',
         sizes=[(16, 16), (32, 32), (48, 48), (64, 64),
                (128, 128), (256, 256)])
img.save('icon_preview.png')
print("图标生成成功")