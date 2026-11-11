"""高德路网瓦片地图 + 飞机标记 + 轨迹。

为了"像软件一样流畅"，做了四件事：
    1. **连续缩放**：缩放级别是浮点数（zoom_f），瓦片按 2^n 缩放绘制，滚轮不跳级
    2. **父级兜底**：新一级瓦片没到之前，用上一级瓦片放大顶上，不会出现白块
    3. **优先级**：缺的瓦片按"离屏幕中心远近"排队，中间的先到
    4. **缓存上限**：内存里最多留 400 张瓦片，超出丢最旧的；磁盘缓存 ~/.fc_osm_tiles

坐标：飞机位置/轨迹用 WGS-84（GPS 原始），地图中心用 GCJ-02（和高德瓦片一致）。
"""

import math
import os
import queue
import urllib.request

from PyQt5.QtCore import QByteArray, QPointF, QRectF, Qt, QThread, pyqtSignal
from PyQt5.QtGui import QColor, QFont, QPainter, QPen, QPixmap, QPolygonF
from PyQt5.QtWidgets import QWidget

TILE_SIZE = 256
CACHE_DIR = os.path.join(os.path.expanduser("~"), ".fc_osm_tiles")

TILE_URL    = "http://webrd0{s}.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x={x}&y={y}&z={z}"
ATTRIBUTION = "© 高德地图"

ZOOM_MIN, ZOOM_MAX = 11.0, 19.0
ZOOM_STEP = 0.35                  # 每格滚轮缩放多少级（小于 1 = 平滑）
SCALE_STEPS = (5000, 2000, 1000, 500, 200, 100, 50)
SCALE_PX = 140.0
TILE_CACHE_MAX = 400              # 内存里最多留多少张
PARENT_FALLBACK = 3               # 最多往上找几级父瓦片


# ---------------------------------------------------------------- 坐标系
def _out_of_china(lat, lon):
    return not (73.66 < lon < 135.05 and 3.86 < lat < 53.55)


def _tlat(x, y):
    return (-100.0 + 2.0*x + 3.0*y + 0.2*y*y + 0.1*x*y + 0.2*math.sqrt(abs(x))
            + (20.0*math.sin(6.0*x*math.pi) + 20.0*math.sin(2.0*x*math.pi))*2.0/3.0
            + (20.0*math.sin(y*math.pi) + 40.0*math.sin(y/3.0*math.pi))*2.0/3.0
            + (160.0*math.sin(y/12.0*math.pi) + 320.0*math.sin(y*math.pi/30.0))*2.0/3.0)


def _tlon(x, y):
    return (300.0 + x + 2.0*y + 0.1*x*x + 0.1*x*y + 0.1*math.sqrt(abs(x))
            + (20.0*math.sin(6.0*x*math.pi) + 20.0*math.sin(2.0*x*math.pi))*2.0/3.0
            + (20.0*math.sin(x*math.pi) + 40.0*math.sin(x/3.0*math.pi))*2.0/3.0
            + (150.0*math.sin(x/12.0*math.pi) + 300.0*math.sin(x/30.0*math.pi))*2.0/3.0)


def wgs84_to_gcj(lat, lon):
    """WGS-84（GPS 原始值）→ GCJ-02（高德用的火星坐标）"""
    if _out_of_china(lat, lon):
        return lat, lon

    a  = 6378245.0
    ee = 0.00669342162296594323

    dlat = _tlat(lon - 105.0, lat - 35.0)
    dlon = _tlon(lon - 105.0, lat - 35.0)

    radlat = lat / 180.0 * math.pi
    magic = 1.0 - ee * math.sin(radlat) ** 2
    sqrtmagic = math.sqrt(magic)

    dlat = (dlat * 180.0) / ((a * (1.0 - ee)) / (magic * sqrtmagic) * math.pi)
    dlon = (dlon * 180.0) / (a / sqrtmagic * math.cos(radlat) * math.pi)

    return lat + dlat, lon + dlon


# ---------------------------------------------------------------- 瓦片下载
class TileFetcher(QThread):
    """后台线程下载瓦片：Python urllib 自带 TLS，不依赖 Qt 的 OpenSSL"""

    tile_ready = pyqtSignal(int, int, int, QByteArray)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.jobs = queue.Queue()
        self.running = True

    def request(self, z, x, y):
        self.jobs.put((z, x, y))

    def clear_pending(self):
        """视野变了，旧位置没下完的瓦片没用了"""
        while True:
            try:
                self.jobs.get_nowait()
            except queue.Empty:
                break

    def run(self):
        while self.running:
            try:
                z, x, y = self.jobs.get(timeout=0.3)
            except queue.Empty:
                continue

            if not self.running:
                break

            path = os.path.join(CACHE_DIR, "amap_%d_%d_%d.png" % (z, x, y))

            if os.path.exists(path):
                try:
                    with open(path, "rb") as f:
                        self.tile_ready.emit(z, x, y, QByteArray(f.read()))
                    continue
                except OSError:
                    pass

            url = TILE_URL.replace("{s}", str((x + y) % 4 + 1)).format(z=z, x=x, y=y)
            req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 FC-Configurator/1.0"})

            try:
                data = urllib.request.urlopen(req, timeout=4).read()
            except Exception:
                continue

            if not data:
                continue

            try:
                os.makedirs(CACHE_DIR, exist_ok=True)
                with open(path, "wb") as f:
                    f.write(data)
            except OSError:
                pass

            self.tile_ready.emit(z, x, y, QByteArray(data))

    def stop(self):
        self.running = False
        if not self.wait(5000):                 # 还在下载就别等它，强停，否则退出时崩
            self.terminate()
            self.wait(500)


# ---------------------------------------------------------------- 地图控件
class OsmMap(QWidget):
    """高德路网地图：连续缩放、拖动平移、双击跟随飞机。"""

    def __init__(self, parent=None, cache_max=TILE_CACHE_MAX):
        super().__init__(parent)

        self.cache_max = cache_max

        self.lat, self.lon = 31.2304, 121.4737     # 飞机位置（WGS-84）
        self.course = 0.0
        self.valid = False
        self.track = []                            # [(lat, lon), ...] WGS-84

        self.zoom_f = 16.0
        self.center_lat, self.center_lon = wgs84_to_gcj(self.lat, self.lon)
        self.follow = True

        self._drag_pos = None
        self._tiles = {}                           # (z,x,y) -> QPixmap（带上限）
        self._requested = set()

        self.text_color = QColor("#666666")
        self.bg_color = QColor("#eef1f4")

        self.fetcher = TileFetcher(self)
        self.fetcher.tile_ready.connect(self._on_tile)
        self.fetcher.start()

        self.setMinimumSize(320, 260)
        self.setMouseTracking(True)

    # ------------------------------------------------------------ 对外
    def set_colors(self, text_color):
        self.text_color = QColor(text_color)
        self.update()

    def set_position(self, lat, lon, course=0.0, valid=True):
        """飞机位置（WGS-84）"""
        self.lat, self.lon = lat, lon
        self.course = course
        self.valid = valid

        if valid:
            self.track.append((lat, lon))
            if len(self.track) > 1500:
                self.track.pop(0)

            if self.follow:
                self.center_lat, self.center_lon = wgs84_to_gcj(lat, lon)

        self.update()

    def center_on(self, lat, lon, zoom=None):
        self.center_lat, self.center_lon = wgs84_to_gcj(lat, lon)

        if zoom is not None:
            self.zoom_f = max(ZOOM_MIN, min(ZOOM_MAX, float(zoom)))

        self._purge_requests()
        self.update()

    # ------------------------------------------------------------ 投影
    @property
    def z(self):
        return int(round(self.zoom_f))

    @property
    def tile_scale(self):
        """瓦片在屏幕上的缩放倍数（连续缩放的来源）"""
        return 2.0 ** (self.zoom_f - self.z)

    @staticmethod
    def _to_tile(lat, lon, z):
        n = 2.0 ** z
        x = (lon + 180.0) / 360.0 * n
        rad = math.radians(max(-85.0, min(85.0, lat)))
        y = (1.0 - math.log(math.tan(rad) + 1.0 / math.cos(rad)) / math.pi) / 2.0 * n
        return x, y

    def _world_px(self, lat, lon):
        tx, ty = self._to_tile(lat, lon, self.z)
        return tx * TILE_SIZE, ty * TILE_SIZE

    def _center_px(self):
        return self._world_px(self.center_lat, self.center_lon)

    def _to_screen(self, lat, lon, left, top):
        wx, wy = self._world_px(lat, lon)
        s = self.tile_scale
        return QPointF((wx - left) * s, (wy - top) * s)

    def _px_to_latlon(self, px, py, left, top):
        """屏幕像素 → GCJ-02 经纬度"""
        s = self.tile_scale
        wx = px / s + left
        wy = py / s + top

        n = 2.0 ** self.z
        tx, ty = wx / TILE_SIZE, wy / TILE_SIZE
        lon = tx / n * 360.0 - 180.0
        lat = math.degrees(math.atan(math.sinh(math.pi * (1.0 - 2.0 * ty / n))))
        return lat, lon

    def _meters_per_px(self):
        return 156543.03392 * math.cos(math.radians(self.center_lat)) / (2.0 ** self.zoom_f)

    # ------------------------------------------------------------ 瓦片
    def _purge_requests(self):
        self.fetcher.clear_pending()
        self._requested.clear()

    def _tile(self, z, x, y):
        return self._tiles.get((z, x, y))

    def _parent_tile(self, z, x, y):
        """往上找已经缓存过的父瓦片，返回 (pixmap, 级差) 或 (None, 0)"""
        for up in range(1, PARENT_FALLBACK + 1):
            pz = z - up
            if pz < 1:
                break

            px, py = x >> up, y >> up
            pix = self._tiles.get((pz, px, py))
            if pix is not None:
                return pix, up

        return None, 0

    def _on_tile(self, z, x, y, data):
        pix = QPixmap()
        if not pix.loadFromData(data):
            return

        self._tiles[(z, x, y)] = pix

        if len(self._tiles) > self.cache_max:
            for key in list(self._tiles.keys())[:len(self._tiles) - self.cache_max]:
                del self._tiles[key]

        self.update()

    # ------------------------------------------------------------ 绘制
    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.setRenderHint(QPainter.SmoothPixmapTransform, True)

        w, h = self.width(), self.height()
        p.fillRect(self.rect(), self.bg_color)

        z = self.z
        s = self.tile_scale
        n = 2 ** z

        cx, cy = self._center_px()
        left, top = cx - w / (2.0 * s), cy - h / (2.0 * s)      # 世界像素坐标

        step = TILE_SIZE * s

        tx0 = int(math.floor(left / TILE_SIZE))
        ty0 = int(math.floor(top / TILE_SIZE))
        tx1 = int(math.floor((left + w / s) / TILE_SIZE))
        ty1 = int(math.floor((top + h / s) / TILE_SIZE))

        missing = []

        for tx in range(tx0, tx1 + 1):
            for ty in range(ty0, ty1 + 1):
                if ty < 0 or ty >= n:
                    continue

                wx, wy = tx % n, ty
                dx = (tx * TILE_SIZE - left) * s
                dy = (ty * TILE_SIZE - top) * s
                rect = QRectF(dx, dy, step + 1.0, step + 1.0)

                pix = self._tile(z, wx, wy)

                if pix is not None:
                    p.drawPixmap(rect, pix, QRectF(0, 0, TILE_SIZE, TILE_SIZE))
                    continue

                # 没到货：先用父级瓦片顶着，避免白块
                parent, up = self._parent_tile(z, wx, wy)
                if parent is not None:
                    span = 1 << up
                    sub = 1.0 / span
                    ox = (wx % span) * sub
                    oy = (wy % span) * sub
                    p.drawPixmap(rect, parent,
                                 QRectF(ox * TILE_SIZE, oy * TILE_SIZE,
                                        TILE_SIZE * sub, TILE_SIZE * sub))
                else:
                    p.fillRect(rect, QColor("#dde3e8"))

                missing.append((wx, wy))

        self._request_tiles(missing, cx, cy, w, h, s)

        self._draw_track(p, left, top)
        self._draw_marker(p, left, top, w, h)
        self._draw_scalebar(p, h)
        self._draw_footer(p, w, h, len(missing))

    def _request_tiles(self, missing, cx, cy, w, h, s):
        """按离屏幕中心的距离排队，中间的先到"""
        if not missing:
            return

        z = self.z
        half = TILE_SIZE / 2.0

        def dist(key):
            x, y = key
            dx = (x * TILE_SIZE + half) - cx
            dy = (y * TILE_SIZE + half) - cy
            return dx * dx + dy * dy

        for key in sorted(missing, key=dist):
            full = (z, key[0], key[1])
            if full in self._requested:
                continue

            self._requested.add(full)
            self.fetcher.request(*full)

    def _draw_track(self, p, left, top):
        if len(self.track) < 2:
            return

        pen = QPen(QColor("#00b894"), 2)
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)
        p.setPen(pen)
        p.drawPolyline(QPolygonF([self._to_screen(la, lo, left, top)
                                  for la, lo in self.track]))

    def _draw_marker(self, p, left, top, w, h):
        if not self.valid:
            return                          # 还没定位就什么都不画（地图本身照常拖）

        la, lo = wgs84_to_gcj(self.lat, self.lon)
        s = self._to_screen(la, lo, left, top)

        p.setPen(QPen(QColor(0, 168, 255, 90), 1))
        p.setBrush(QColor(0, 168, 255, 30))
        p.drawEllipse(s, 16, 16)

        a = math.radians(self.course)
        tip    = QPointF(s.x() + 11 * math.sin(a),      s.y() - 11 * math.cos(a))
        leftp  = QPointF(s.x() + 7 * math.sin(a + 2.5), s.y() - 7 * math.cos(a + 2.5))
        rightp = QPointF(s.x() + 7 * math.sin(a - 2.5), s.y() - 7 * math.cos(a - 2.5))

        p.setPen(QPen(QColor("#0078d4"), 2))
        p.setBrush(QColor("#00d7ff"))
        p.drawPolygon(QPolygonF([tip, leftp, rightp]))

    def _scale(self):
        mpp = self._meters_per_px()
        best = min(SCALE_STEPS, key=lambda m: abs(m / mpp - SCALE_PX))
        return best, best / mpp

    def _draw_scalebar(self, p, h):
        meters, px = self._scale()

        x0, y0 = 16.0, h - 26.0

        p.setPen(QPen(QColor(60, 60, 60, 200), 2))
        p.setBrush(Qt.NoBrush)
        p.drawLine(QPointF(x0, y0), QPointF(x0 + px, y0))
        p.drawLine(QPointF(x0, y0 - 5), QPointF(x0, y0 + 5))
        p.drawLine(QPointF(x0 + px, y0 - 5), QPointF(x0 + px, y0 + 5))

        f = QFont(p.font())
        f.setPointSize(9)
        p.setFont(f)
        p.setPen(QPen(self.text_color, 1))

        text = "%d km" % (meters // 1000) if meters >= 1000 else "%d m" % meters
        p.drawText(QRectF(x0, y0 - 22, px, 16), Qt.AlignCenter, text)

    def _draw_footer(self, p, w, h, missing):
        f = QFont(p.font())
        f.setPointSize(8)
        p.setFont(f)
        p.setPen(QPen(self.text_color, 1))

        if missing > 0:
            p.drawText(QRectF(0, h - 18, w - 110, 16),
                       Qt.AlignRight | Qt.AlignVCenter, "瓦片加载中…")

        p.drawText(QRectF(0, h - 18, w - 8, 16),
                   Qt.AlignRight | Qt.AlignVCenter, ATTRIBUTION)

    # ------------------------------------------------------------ 交互
    def wheelEvent(self, event):
        """以鼠标位置为锚点连续缩放"""
        step = ZOOM_STEP if event.angleDelta().y() > 0 else -ZOOM_STEP
        target = max(ZOOM_MIN, min(ZOOM_MAX, self.zoom_f + step))

        if abs(target - self.zoom_f) < 1e-6:
            return

        w, h = self.width(), self.height()
        cx, cy = self._center_px()
        s = self.tile_scale
        pos = event.pos()
        lat0, lon0 = self._px_to_latlon(pos.x(), pos.y(),
                                        cx - w / (2.0 * s), cy - h / (2.0 * s))

        self.zoom_f = target

        cx, cy = self._center_px()
        s = self.tile_scale
        lat1, lon1 = self._px_to_latlon(pos.x(), pos.y(),
                                        cx - w / (2.0 * s), cy - h / (2.0 * s))

        self.center_lat += lat0 - lat1
        self.center_lon += lon0 - lon1

        if self.follow and self.valid:
            self.follow = False

        self._purge_requests()
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

        mpp = self._meters_per_px()
        coslat = math.cos(math.radians(self.center_lat))

        self.center_lat += dy * mpp / 110540.0
        self.center_lon -= dx * mpp / (111320.0 * max(0.1, coslat))

        self._purge_requests()
        self.update()

    def mouseReleaseEvent(self, event):
        self._drag_pos = None

    def mouseDoubleClickEvent(self, event):
        self.follow = True

        if self.valid:
            self.center_lat, self.center_lon = wgs84_to_gcj(self.lat, self.lon)

        self._purge_requests()
        self.update()