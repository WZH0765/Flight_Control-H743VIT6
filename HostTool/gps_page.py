"""GPS：独立的 Gps 页（整窗高德大地图）。

数据来源优先级：
    1. 固件发来的 `GPS,...` 帧（有定位就自动切过去）
    2. 没有固件数据时，用 IP 定位把地图挪到本机大致位置（城市级精度，差几公里）

仿真页用的是 `virtual_map.py` 的虚拟网格地图，不联网；这张真地图留给以后接 GPS 模块用。
"""

import json
import urllib.request

from PyQt5.QtCore import QThread, pyqtSignal

# 免费、无需 key 的 IP 定位（按顺序试）
IP_SERVICES = (
    "http://ip-api.com/json/?fields=status,lat,lon,city",
    "https://ipapi.co/json/",
    "https://ipinfo.io/json",
)


class IpLocator(QThread):
    """查本机大致经纬度（不阻塞界面）"""

    found = pyqtSignal(float, float, str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._stop = False

    def stop(self):
        self._stop = True

    def run(self):
        for url in IP_SERVICES:
            if self._stop:
                return

            try:
                req = urllib.request.Request(url, headers={"User-Agent": "FC-Configurator/1.0"})
                data = json.loads(urllib.request.urlopen(req, timeout=1.5).read().decode("utf-8", "ignore"))
            except Exception:
                continue

            lat = data.get("lat") or data.get("latitude")
            lon = data.get("lon") or data.get("longitude")

            if lat is None or lon is None:
                loc = data.get("loc")                    # ipinfo 格式 "31.23,121.47"
                if isinstance(loc, str) and "," in loc:
                    lat, lon = loc.split(",", 1)

            try:
                self.found.emit(float(lat), float(lon), data.get("city", "") or "")
                return
            except (TypeError, ValueError):
                continue


class GpsMixin:
    """给 MainWindow 提供两块地图、一行状态、IP 兜底定位和 `GPS,...` 解析。"""

    _gps_state = {"fix": 0, "sats": 0, "lat": 0.0, "lon": 0.0,
                  "alt": 0.0, "speed": 0.0, "course": 0.0}

    # ------------------------------------------------------------ 独立的 Gps 页
    def _build_gps_page(self):
        """页面和导航按钮都在 .ui 里，这里只算索引、连信号"""
        self._page_gps_idx = self.stackedWidget.indexOf(self.page_gps)

        self.gps_map_big.cache_max = 150              # 只有一块屏，缓存小一点
        self.btn_gps.clicked.connect(lambda _, i=self._page_gps_idx: self._switch_page(i))

        self._init_gps()

    def _init_gps(self):
        self._locator = None
        self._gps_zoomed = False                 # 第一次拿到定位就自动放大到飞机
        self._locate_me()                        # 先按 IP 把地图挪到本机附近

    # ------------------------------------------------------------ IP 定位
    def _locate_me(self):
        """按 IP 查本机大致位置（城市级），固件有真定位后以固件为准"""
        if self._locator is not None and self._locator.isRunning():
            return

        self._locator = IpLocator(self)
        self._locator.found.connect(self._on_located)
        self._locator.start()

    def _on_located(self, lat, lon, city):
        if self._gps_state["fix"] >= 2:
            return                              # 固件已经有真定位，不用 IP 的

        self.gps_map_big.center_on(lat, lon, zoom=15)

    # ------------------------------------------------------------ 数据
    def _handle_gps_line(self, line: str):
        """GPS,<fix>,<sats>,<lat*1e7>,<lon*1e7>,<alt_cm>,<speed_cm/s>,<course_deg>"""
        parts = line.split(",")
        if len(parts) != 8:
            return

        try:
            fix    = int(parts[1])
            sats   = int(parts[2])
            lat    = int(parts[3]) / 1e7
            lon    = int(parts[4]) / 1e7
            alt    = int(parts[5]) / 100.0
            speed  = int(parts[6])
            course = int(parts[7])
        except ValueError:
            return

        self._gps_state.update(fix=fix, sats=sats, lat=lat, lon=lon,
                               alt=alt, speed=speed, course=course)

        valid = fix >= 2

        self.gps_map_big.set_position(lat, lon, course, valid)

        if valid and not self._gps_zoomed:
            self._gps_zoomed = True

            self.gps_map_big.zoom_f = max(self.gps_map_big.zoom_f, 17.0)
            self.gps_map_big.follow = True
            self.gps_map_big.center_on(lat, lon)