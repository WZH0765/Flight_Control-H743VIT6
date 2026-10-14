import math
from PyQt5.QtWidgets import QWidget
from PyQt5.QtGui import QPainter, QColor, QPen, QPainterPath
from PyQt5.QtCore import Qt, QRectF


class AttitudeIndicator(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.roll  = 0.0
        self.pitch = 0.0
        self.yaw   = 0.0

        # 主题颜色
        self.bg_color     = QColor("#0f1419")
        self.border_color = QColor("#2a3a4a")
        self.text_color   = QColor("#e8eaed")
        self.line_color   = QColor("#e8eaed")   # 地平线/刻度
        self.accent_color = QColor("#00d4ff")   # 中心飞机
        self.yaw_color    = QColor("#ff4757")   # Yaw 指针

    def set_attitude(self, roll, pitch, yaw):
        self.roll  = roll
        self.pitch = pitch
        self.yaw   = yaw
        self.update()

    def set_colors(self, bg, border, text):
        self.bg_color     = QColor(bg)
        self.border_color = QColor(border)
        self.text_color   = QColor(text)
        self.line_color   = QColor(text)

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)

        w, h = self.width(), self.height()
        size = min(w, h)
        cx, cy = w / 2, h / 2
        R = size / 2 - 8

        # 背景
        p.fillRect(0, 0, w, h, self.bg_color)

        # 裁剪成圆
        clip = QPainterPath()
        clip.addEllipse(QRectF(cx - R, cy - R, 2*R, 2*R))
        p.setClipPath(clip)

        # ============ 刻度（随 Roll 旋转、随 Pitch 平移） ============
        p.save()
        p.translate(cx, cy)
        p.rotate(-self.roll)

        # Pitch 满量程 ±45°
        pitch_px = self.pitch * R / 45.0
        p.translate(0, pitch_px)

        # 画刻度线：每 10° 一条，从 -40° 到 +40°
        for ang in range(-40, 50, 10):
            y = -ang * R / 45.0
            if abs(y) > R:
                continue

            # 刻度长度：主刻度（整 20°）长，副刻度短
            if ang == 0:
                # 地平线：最长
                half = R * 0.75
                pen = QPen(self.line_color, 2)
            elif ang % 20 == 0:
                half = R * 0.35
                pen = QPen(self.line_color, 2)
            else:
                half = R * 0.15
                pen = QPen(self.line_color, 1)

            p.setPen(pen)
            p.drawLine(int(-half), int(y), int(half), int(y))

            # 主刻度两侧标注角度数字
            if ang != 0 and ang % 20 == 0:
                p.setPen(QPen(self.text_color, 1))
                font = p.font()
                font.setPointSize(7)
                p.setFont(font)
                p.drawText(int(-half - 22), int(y - 8), 20, 16,
                           Qt.AlignRight | Qt.AlignVCenter, str(abs(ang)))
                p.drawText(int(half + 2), int(y - 8), 20, 16,
                           Qt.AlignLeft | Qt.AlignVCenter, str(abs(ang)))

        p.restore()

        # ============ 中心固定飞机符号 ============
        p.setPen(QPen(self.accent_color, 3))
        p.setBrush(Qt.NoBrush)

        # 左横臂
        p.drawLine(int(cx - R*0.4), int(cy), int(cx - R*0.15), int(cy))
        # 右横臂
        p.drawLine(int(cx + R*0.15), int(cy), int(cx + R*0.4), int(cy))
        # 中心小圆点
        p.setBrush(self.accent_color)
        p.drawEllipse(QRectF(cx - 3, cy - 3, 6, 6))

        # 机翼末端小竖线（像 AP 那种）
        p.setBrush(Qt.NoBrush)
        p.setPen(QPen(self.accent_color, 2))
        p.drawLine(int(cx - R*0.4), int(cy - R*0.06),
                   int(cx - R*0.4), int(cy + R*0.02))
        p.drawLine(int(cx + R*0.4), int(cy - R*0.06),
                   int(cx + R*0.4), int(cy + R*0.02))

        # ============ 外圈 ============
        p.setPen(QPen(self.border_color, 2))
        p.setBrush(Qt.NoBrush)
        p.drawEllipse(QRectF(cx - R, cy - R, 2*R, 2*R))

        # ============ Yaw 刻度 & 指针 ============
        # 圆周小刻线：每 30°
        p.setPen(QPen(self.text_color, 1))
        for deg in range(0, 360, 30):
            a = math.radians(deg - 90)
            r1 = R * 0.96
            r2 = R * 0.90
            x1 = cx + r1 * math.cos(a); y1 = cy + r1 * math.sin(a)
            x2 = cx + r2 * math.cos(a); y2 = cy + r2 * math.sin(a)
            p.drawLine(int(x1), int(y1), int(x2), int(y2))

        # 顶部 Yaw 三角指针
        p.setBrush(self.yaw_color)
        p.setPen(Qt.NoPen)
        angle = math.radians(self.yaw - 90)
        tip_x = cx + R * 0.92 * math.cos(angle)
        tip_y = cy + R * 0.92 * math.sin(angle)
        perp = angle + math.pi/2
        base_w = R * 0.05
        p1 = (tip_x, tip_y)
        p2 = (tip_x - R*0.12*math.cos(angle) + base_w*math.cos(perp),
              tip_y - R*0.12*math.sin(angle) + base_w*math.sin(perp))
        p3 = (tip_x - R*0.12*math.cos(angle) - base_w*math.cos(perp),
              tip_y - R*0.12*math.sin(angle) - base_w*math.sin(perp))
        tri = QPainterPath()
        tri.moveTo(*p1)
        tri.lineTo(*p2)
        tri.lineTo(*p3)
        tri.closeSubpath()
        p.drawPath(tri)