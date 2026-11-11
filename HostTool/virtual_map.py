"""仿真专用的虚拟地图：米制网格 + 飞行轨迹，不联网、不下载。

坐标是 NED 水平位置（米）：**北朝上、东朝右**，原点 = 起飞点。
和真实地图（`osm_map.py`）的区别：这里一格就是多少米，看得见距离，加载瞬间完成。
"""

import math
import time

from PyQt5.QtCore import QPointF, QRectF, Qt
from PyQt5.QtGui import QColor, QFont, QPainter, QPen, QPolygonF
from PyQt5.QtWidgets import QWidget

GRID_STEPS = (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000)
GRID_PX    = 90.0                   # 一格大约占多少像素
ZOOM_MIN, ZOOM_MAX = 0.02, 5.0      # 米/像素
ZOOM_STEP  = 1.25
TRACK_MAX  = 3000
TRACK_MIN_DIST = 0.05               # 轨迹点最小间隔（米），防止悬停时灌满


class VirtualMap(QWidget):
    """虚拟地图：网格 + 轨迹 + 飞机标记，可滚轮缩放、拖动平移、双击跟随。"""

    def __init__(self, parent=None):
        super().__init__(parent)

        self.n, self.e = 0.0, 0.0       # 飞机位置（北/东，米）
        self.alt = 0.0                  # 高度（米）
        self.course = 0.0               # 航向（度，0=北）
        self.valid = False
        self.track = []                 # [(n, e), ...]
        self.vn, self.ve = 0.0, 0.0     # 水平速度（m/s），由相邻两帧差分出来
        self._last = None

        self.mpp = 0.1                  # 米/像素（视野约 90m 宽，几米的移动也看得出）
        self.cn, self.ce = 0.0, 0.0     # 视野中心
        self.follow = True

        self._drag_pos = None

        self.bg_color   = QColor("#ffffff")
        self.grid_color = QColor("#d0d7de")
        self.text_color = QColor("#666666")

        self.setMinimumHeight(150)
        self.setMouseTracking(True)

    # ------------------------------------------------------------ 对外
    def set_colors(self, bg, grid, text):
        self.bg_color   = QColor(bg)
        self.grid_color = QColor(grid)
        self.text_color = QColor(text)
        self.update()

    def set_position(self, n, e, course, alt, valid=True):
        self.n, self.e = n, e
        self.course, self.alt = course, alt
        self.valid = valid

        now = time.monotonic()
        if self._last is not None:
            dt = now - self._last[0]
            if 1e-3 < dt < 1.0:
                self.vn = (n - self._last[1]) / dt
                self.ve = (e - self._last[2]) / dt
        self._last = (now, n, e)

        if not valid:
            self.update()
            return

        last = self.track[-1] if self.track else None
        if last is None or math.hypot(n - last[0], e - last[1]) >= TRACK_MIN_DIST:
            self.track.append((n, e))
            if len(self.track) > TRACK_MAX:
                self.track.pop(0)

        if self.follow:
            self.cn, self.ce = n, e

        self.update()

    # ------------------------------------------------------------ 坐标换算
    def _pt(self, n, e):
        return QPointF(self.width() / 2.0 + (e - self.ce) / self.mpp,
                       self.height() / 2.0 - (n - self.cn) / self.mpp)

    def _grid_step(self):
        """挑一个"整数米"的格宽，让格子尽量接近 GRID_PX 像素"""
        target = GRID_PX * self.mpp
        best = None

        for k in range(-1, 4):
            for m in (1, 2, 5):
                s = m * (10.0 ** k)
                if best is None or abs(s - target) < abs(best - target):
                    best = s

        return best

    def _screen_to_ne(self, px, py):
        return (self.cn + (self.height() / 2.0 - py) * self.mpp,
                self.ce + (px - self.width() / 2.0) * self.mpp)

    # ------------------------------------------------------------ 绘制
    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)

        w, h = self.width(), self.height()
        p.fillRect(self.rect(), self.bg_color)

        step = self._grid_step()
        self._draw_grid(p, w, h, step)
        self._draw_track(p)
        self._draw_home(p)
        self._draw_aircraft(p)
        self._draw_scalebar(p, h, step)
        self._draw_north(p, w)

    def _draw_grid(self, p, w, h, step):
        f = QFont(p.font())
        f.setPointSize(8)
        p.setFont(f)

        minor = QColor(self.grid_color)
        minor.setAlpha(110)
        major = QColor(self.grid_color)
        major.setAlpha(210)

        half_e = w / 2.0 * self.mpp
        half_n = h / 2.0 * self.mpp

        for i in range(int(math.floor((self.ce - half_e) / step)),
                       int(math.ceil((self.ce + half_e) / step)) + 1):
            e = i * step
            x = w / 2.0 + (e - self.ce) / self.mpp
            is_major = (i % 5 == 0)

            p.setPen(QPen(major if is_major else minor, 1))
            p.drawLine(QPointF(x, 0), QPointF(x, h))

            if is_major:
                p.setPen(QPen(self.text_color, 1))
                p.drawText(QRectF(x + 3, 2, 64, 14), Qt.AlignLeft | Qt.AlignVCenter,
                           "%d" % round(e))

        for j in range(int(math.floor((self.cn - half_n) / step)),
                       int(math.ceil((self.cn + half_n) / step)) + 1):
            n = j * step
            y = h / 2.0 - (n - self.cn) / self.mpp
            is_major = (j % 5 == 0)

            p.setPen(QPen(major if is_major else minor, 1))
            p.drawLine(QPointF(0, y), QPointF(w, y))

            if is_major:
                p.setPen(QPen(self.text_color, 1))
                p.drawText(QRectF(3, y + 1, 60, 14), Qt.AlignLeft | Qt.AlignVCenter,
                           "%d" % round(n))

    def _draw_track(self, p):
        if len(self.track) < 2:
            return

        pen = QPen(QColor("#00b894"), 2)
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)
        p.setPen(pen)
        p.drawPolyline(QPolygonF([self._pt(n, e) for n, e in self.track]))

    def _draw_home(self, p):
        """起飞点：一个空心小方框"""
        s = self._pt(0.0, 0.0)

        p.setPen(QPen(QColor(self.text_color), 1, Qt.DashLine))
        p.setBrush(Qt.NoBrush)
        p.drawRect(QRectF(s.x() - 5, s.y() - 5, 10, 10))

        p.setPen(QPen(self.text_color, 1))
        p.drawText(QRectF(s.x() + 7, s.y() - 8, 40, 14), Qt.AlignLeft | Qt.AlignVCenter, "H")

    def _draw_aircraft(self, p):
        if not self.valid:
            return

        s = self._pt(self.n, self.e)
        a = math.radians(self.course)

        # 速度矢量：跟着不跟随时，靠这根箭头也能看出在往哪飞
        sp = math.hypot(self.vn, self.ve)
        if sp > 0.08:
            length = min(sp * 1.5 / self.mpp, 90.0)
            ux, uy = self.ve / sp, -self.vn / sp
            tip = QPointF(s.x() + ux * length, s.y() + uy * length)

            p.setPen(QPen(QColor("#ffa502"), 2))
            p.drawLine(s, tip)

            ang = math.atan2(uy, ux)
            for d in (0.45, -0.45):
                p.drawLine(tip, QPointF(tip.x() - 9 * math.cos(ang + d),
                                        tip.y() - 9 * math.sin(ang + d)))

        p.setPen(QPen(QColor(0, 168, 255, 90), 1))
        p.setBrush(QColor(0, 168, 255, 30))
        p.drawEllipse(s, 15, 15)

        tip    = QPointF(s.x() + 11 * math.sin(a),      s.y() - 11 * math.cos(a))
        leftp  = QPointF(s.x() + 7 * math.sin(a + 2.5), s.y() - 7 * math.cos(a + 2.5))
        rightp = QPointF(s.x() + 7 * math.sin(a - 2.5), s.y() - 7 * math.cos(a - 2.5))

        p.setPen(QPen(QColor("#0078d4"), 2))
        p.setBrush(QColor("#00d7ff"))
        p.drawPolygon(QPolygonF([tip, leftp, rightp]))

        f = QFont(p.font())
        f.setPointSize(8)
        p.setFont(f)
        p.setPen(QPen(self.text_color, 1))
        p.drawText(QRectF(s.x() - 40, s.y() + 15, 80, 14),
                   Qt.AlignHCenter | Qt.AlignVCenter, "%.1f m" % self.alt)

    def _draw_scalebar(self, p, h, step):
        meters, px = self._nice_scale()

        x0, y0 = 14.0, h - 18.0

        p.setPen(QPen(QColor(120, 120, 120, 200), 2))
        p.setBrush(Qt.NoBrush)
        p.drawLine(QPointF(x0, y0), QPointF(x0 + px, y0))
        p.drawLine(QPointF(x0, y0 - 4), QPointF(x0, y0 + 4))
        p.drawLine(QPointF(x0 + px, y0 - 4), QPointF(x0 + px, y0 + 4))

        f = QFont(p.font())
        f.setPointSize(8)
        p.setFont(f)
        p.setPen(QPen(self.text_color, 1))

        text = "%d km" % (meters // 1000) if meters >= 1000 else "%d m" % meters
        p.drawText(QRectF(x0, y0 - 17, max(px, 40), 14), Qt.AlignCenter, text)

    def _nice_scale(self):
        """比例尺：挑一个整数米，让长度尽量接近 110px"""
        target = 110.0 * self.mpp
        best = min(GRID_STEPS, key=lambda s: abs(s - target))

        return best, best / self.mpp

    def _draw_north(self, p, w):
        x, y = w - 24.0, 34.0

        p.setPen(QPen(self.text_color, 1))
        p.setBrush(Qt.NoBrush)
        p.drawLine(QPointF(x, y + 6), QPointF(x, y - 6))
        p.drawLine(QPointF(x, y - 6), QPointF(x - 3.5, y - 1))
        p.drawLine(QPointF(x, y - 6), QPointF(x + 3.5, y - 1))

        f = QFont(p.font())
        f.setPointSize(8)
        f.setBold(True)
        p.setFont(f)
        p.drawText(QRectF(x - 20, y - 26, 40, 14), Qt.AlignCenter, "N")

    # ------------------------------------------------------------ 交互
    def wheelEvent(self, event):
        """以鼠标位置为锚点缩放"""
        step = ZOOM_STEP if event.angleDelta().y() > 0 else 1.0 / ZOOM_STEP
        target = max(ZOOM_MIN, min(ZOOM_MAX, self.mpp / step))

        if abs(target - self.mpp) < 1e-9:
            return

        pos = event.pos()
        n0, e0 = self._screen_to_ne(pos.x(), pos.y())

        self.mpp = target

        n1, e1 = self._screen_to_ne(pos.x(), pos.y())
        self.cn += n0 - n1
        self.ce += e0 - e1

        if self.follow and self.valid:
            self.follow = False

        self.update()

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._drag_pos = event.pos()
            self.follow = False

    def mouseMoveEvent(self, event):
        if self._drag_pos is None:
            return

        dx = event.pos().x() - self._drag_pos.x()
        dy = event.pos().y() - self._drag_pos.y()
        self._drag_pos = event.pos()

        self.ce -= dx * self.mpp
        self.cn += dy * self.mpp

        self.update()

    def mouseReleaseEvent(self, event):
        self._drag_pos = None

    def mouseDoubleClickEvent(self, event):
        self.follow = True

        if self.valid:
            self.cn, self.ce = self.n, self.e

        self.update()