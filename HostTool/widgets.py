"""自绘控件：IMU 校准卡片（FaceIcon/FaceTile）和虚拟摇杆（StickPad）。"""

from PyQt5.QtCore import Qt, QPointF, QRectF
from PyQt5.QtGui import QColor, QPainter, QPen, QPolygonF
from PyQt5.QtWidgets import QFrame, QLabel, QVBoxLayout, QWidget

# ---------------- 校准姿态说明
FACE_TEXT = {
    1: "+Z 朝下（箭头面朝上,平放）",
    2: "-Z 朝下（箭头面朝下,倒扣）",
    3: "+X 朝下（箭头面朝前,铅垂）",
    4: "-X 朝下（箭头面朝后,铅垂）",
    5: "+Y 朝下（箭头面朝右,铅垂）",
    6: "-Y 朝下（箭头面朝左,铅垂）",
}

FACE_SHORT = {
    1: "+Z 箭头面朝上",
    2: "-Z 箭头面朝下",
    3: "+X 箭头面朝前",
    4: "-X 箭头面朝后",
    5: "+Y 箭头面朝右",
    6: "-Y 箭头面朝左",
}

FACE_COLOR = "#4FC3F7"


# ---------------------------------------------------------------- 校准卡片
class FaceIcon(QWidget):
    def __init__(self, face: int, parent=None):
        super().__init__(parent)
        self.face = face
        self.state = "idle"
        self.setMinimumSize(120, 72)

    def set_state(self, state: str):
        self.state = state
        self.update()

    def _draw_base(self, painter, scale, ox, oy, c_line):
        def P(x, y):
            return QPointF(ox + x*scale, oy + y*scale)

        pen = QPen(QColor(140, 150, 165, 110))
        pen.setWidthF(max(1.0, 1.2*scale))
        pen.setStyle(Qt.DashLine)
        painter.setPen(pen)
        painter.drawLine(P(30, 168), P(230, 168))

        pen = QPen(c_line)
        pen.setWidthF(max(1.2, 1.6*scale))
        painter.setPen(pen)
        painter.setBrush(Qt.NoBrush)

        return P

    def _paint_board(self, painter, scale, ox, oy, c_line):
        P = self._draw_base(painter, scale, ox, oy, c_line)

        painter.drawPolygon(QPolygonF([P(75, 20), P(205, 20), P(205, 150), P(75, 150)]))

        if self.face in (1, 4):
            painter.drawPolygon(QPolygonF([P(140, 42), P(105, 125), P(140, 108), P(175, 125)]))
        else:
            painter.drawPolygon(QPolygonF([P(115, 60), P(165, 60), P(165, 110), P(115, 110)]))

    def _paint_edge(self, painter, scale, ox, oy, c_line):
        P = self._draw_base(painter, scale, ox, oy, c_line)

        painter.drawPolygon(QPolygonF([P(125, 20), P(145, 20), P(145, 150), P(125, 150)]))

        if self.face == 5:
            painter.drawPolygon(QPolygonF([P(105, 65), P(125, 65), P(125, 100), P(105, 100)]))
        else:
            painter.drawPolygon(QPolygonF([P(145, 65), P(165, 65), P(165, 100), P(145, 100)]))

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        scale = min(self.width()/260.0, self.height()/180.0)
        ox = (self.width() - 260*scale)*0.5
        oy = (self.height() - 180*scale)*0.5

        if self.state == "done":
            c_line = QColor(FACE_COLOR)
        else:
            c_line = QColor("#8C96A5")

        if self.face in (1, 2, 3, 4):
            self._paint_board(painter, scale, ox, oy, c_line)
        else:
            self._paint_edge(painter, scale, ox, oy, c_line)

        painter.end()


class SensorIcon(QWidget):
    """22×22 的单线图标，参考 INAV 的传感器图形语言：
    陀螺=原子轨道、加计=三轴箭头、磁=指北箭头+N、气压=温度计、
    光流=镜头+波纹、GPS=卫星"""

    def __init__(self, kind: str, parent=None):
        super().__init__(parent)
        self.kind = kind
        self.color = QColor("#22b8cf")
        self.setFixedSize(24, 24)

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)

        pen = QPen(self.color, 1.4)
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)
        p.setPen(pen)
        p.setBrush(Qt.NoBrush)

        c = self.rect().center()
        x, y = c.x() + 0.5, c.y() + 0.5

        if self.kind == "gyr":                      # 原子轨道
            for angle in (35, -35):
                p.save()
                p.translate(x, y)
                p.rotate(angle)
                p.drawEllipse(QRectF(-7.5, -3.4, 15, 6.8))
                p.restore()

            p.setBrush(self.color)
            p.drawEllipse(QPointF(x, y), 1.3, 1.3)
        elif self.kind == "acc":                    # 三轴箭头
            p.drawLine(QPointF(x, y + 3), QPointF(x, y - 7))
            p.drawLine(QPointF(x, y - 7.5), QPointF(x - 2.6, y - 4))
            p.drawLine(QPointF(x, y - 7.5), QPointF(x + 2.6, y - 4))

            for dx in (-1, 1):
                p.drawLine(QPointF(x, y + 3), QPointF(x + dx * 6.2, y + 7.4))
                p.drawLine(QPointF(x + dx * 6.6, y + 7.6), QPointF(x + dx * 5.0, y + 4.2))
                p.drawLine(QPointF(x + dx * 6.6, y + 7.6), QPointF(x + dx * 3.0, y + 6.2))
        elif self.kind == "mag":                    # 指北箭头 + N
            p.drawLine(QPointF(x, y - 9), QPointF(x, y - 2))
            p.drawLine(QPointF(x, y - 9), QPointF(x - 2.6, y - 5))
            p.drawLine(QPointF(x, y - 9), QPointF(x + 2.6, y - 5))

            # N 也用线画，和整体单线风格一致
            nx, ny = x, y + 4.5
            p.drawLine(QPointF(nx - 3, ny + 3.2), QPointF(nx - 3, ny - 3.2))
            p.drawLine(QPointF(nx - 3, ny - 3.2), QPointF(nx + 3, ny + 3.2))
            p.drawLine(QPointF(nx + 3, ny + 3.2), QPointF(nx + 3, ny - 3.2))
        elif self.kind == "bar":                    # 温度计
            p.drawLine(QPointF(x, y - 7.5), QPointF(x, y + 1.6))
            p.drawEllipse(QPointF(x, y + 4.2), 2.6, 2.6)

            for ty in (-4.5, -1.5, 1.5):
                p.drawLine(QPointF(x - 5, y + ty), QPointF(x - 2.6, y + ty))
        elif self.kind == "ofl":                    # 镜头 + 下方波纹
            p.drawRoundedRect(QRectF(x - 7, y - 8.5, 14, 9), 1.6, 1.6)
            p.drawEllipse(QPointF(x, y - 4), 2.3, 2.3)

            for i, half in enumerate((2.4, 4.4, 6.4)):
                p.drawArc(QRectF(x - half, y + 1.6 - i * 0.4, half * 2, 6),
                          200 * 16, 140 * 16)
        elif self.kind == "gps":                    # 定位针
            p.drawEllipse(QPointF(x, y - 3), 4.6, 4.6)
            p.drawLine(QPointF(x - 3.5, y + 0.6), QPointF(x, y + 8))
            p.drawLine(QPointF(x + 3.5, y + 0.6), QPointF(x, y + 8))
            p.setBrush(self.color)
            p.drawEllipse(QPointF(x, y - 3), 1.3, 1.3)


class FaceTile(QFrame):

    def __init__(self, face: int, parent=None):
        super().__init__(parent)
        self.setObjectName("faceTile")
        self.setProperty("state", "idle")
        self.setFixedSize(168, 124)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 3, 4, 3)
        layout.setSpacing(1)

        self.icon = FaceIcon(face)
        self.icon.setFixedHeight(88)

        self.title = QLabel(f"FACE{face}  {FACE_SHORT[face]}")
        self.title.setAlignment(Qt.AlignCenter)
        self.title.setStyleSheet("font-size: 10pt;")

        layout.addWidget(self.icon)
        layout.addWidget(self.title)

        self.set_state("idle")

    def set_state(self, state: str):
        self.setProperty("state", state)
        self.style().unpolish(self)
        self.style().polish(self)
        self.icon.set_state(state)

        if state in ("active", "done"):
            self.title.setStyleSheet(f"font-size: 10pt; color: {FACE_COLOR};")
        else:
            self.title.setStyleSheet("font-size: 10pt;")


# ---------------------------------------------------------------- 虚拟摇杆
class StickPad(QWidget):
    PAD_W, PAD_H = 150, 150
    MARGIN_L, MARGIN_R = 4, 4
    MARGIN_T, MARGIN_B = 4, 4

    def __init__(self, y_top: str, y_bottom: str, x_left: str, x_right: str,
                 spring_x: bool = True, spring_y: bool = True, parent=None):
        super().__init__(parent)
        self.x = 0.0
        self.y = 0.0
        self.spring_x = spring_x
        self.spring_y = spring_y
        self.y_top = y_top
        self.y_bottom = y_bottom
        self.x_left = x_left
        self.x_right = x_right

        # 配色跟主题走，默认深色
        self.c_bg     = QColor("#1a2332")
        self.c_border = QColor("#2a3a4a")
        self.c_text   = QColor("#8899a6")

        self.setFixedSize(self.PAD_W, self.PAD_H)

    def set_theme(self, bg: str, border: str, text: str):
        self.c_bg     = QColor(bg)
        self.c_border = QColor(border)
        self.c_text   = QColor(text)
        self.update()

    def _area(self):
        return QRectF(self.MARGIN_L, self.MARGIN_T,
                      self.width() - self.MARGIN_L - self.MARGIN_R,
                      self.height() - self.MARGIN_T - self.MARGIN_B)

    def _to_pos(self):
        a = self._area()
        return (a.center().x() + self.x / 100.0 * (a.width() / 2.0 - 10.0),
                a.center().y() + self.y / 100.0 * (a.height() / 2.0 - 10.0))

    def _set_from_pos(self, pos):
        a = self._area()
        self.x = max(-100.0, min(100.0, (pos.x() - a.center().x()) / (a.width() / 2.0 - 10.0) * 100.0))
        self.y = max(-100.0, min(100.0, (pos.y() - a.center().y()) / (a.height() / 2.0 - 10.0) * 100.0))
        self.update()

    def mousePressEvent(self, event):
        self._set_from_pos(event.pos())

    def mouseMoveEvent(self, event):
        if event.buttons() & Qt.LeftButton:
            self._set_from_pos(event.pos())

    def mouseReleaseEvent(self, event):
        if self.spring_x:
            self.x = 0.0
        if self.spring_y:
            self.y = 0.0
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)

        a = self._area()

        # 摇杆区域（底色/边框跟主题）
        painter.setPen(QPen(self.c_border, 2))
        painter.setBrush(self.c_bg)
        painter.drawRoundedRect(a, 8, 8)

        # 只有中间一横一竖
        painter.setPen(QPen(self.c_border, 1))
        painter.drawLine(QPointF(a.center().x(), a.top()), QPointF(a.center().x(), a.bottom()))
        painter.drawLine(QPointF(a.left(), a.center().y()), QPointF(a.right(), a.center().y()))

        # 四条边的刻度：四等分（25%/50%/75%），中间那格加长
        tick = QColor(self.c_text)
        tick.setAlpha(110)
        painter.setPen(QPen(tick, 1))

        for i in range(1, 4):
            long_tick = (i == 2)
            length = 8.0 if long_tick else 5.0

            fx = a.left() + a.width() * i / 4.0
            fy = a.top() + a.height() * i / 4.0

            painter.drawLine(QPointF(fx, a.top() + 2), QPointF(fx, a.top() + 2 + length))
            painter.drawLine(QPointF(fx, a.bottom() - 2), QPointF(fx, a.bottom() - 2 - length))
            painter.drawLine(QPointF(a.left() + 2, fy), QPointF(a.left() + 2 + length, fy))
            painter.drawLine(QPointF(a.right() - 2, fy), QPointF(a.right() - 2 - length, fy))

        # 摇杆球
        px, py = self._to_pos()
        painter.setPen(QPen(QColor("#00d7ff"), 2))
        painter.setBrush(QColor("#0078d4"))
        painter.drawEllipse(QPointF(px, py), 12, 12)