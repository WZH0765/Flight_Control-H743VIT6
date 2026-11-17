import pyqtgraph as pg
from collections import deque
from config import DATA_LEN, DBG_FIELDS
from PyQt5.QtWidgets import QGridLayout, QLabel

DBG_PLOT_H  = 130

DBG_COLOR = {
    "x": "#ff6b81", "y": "#2ed573", "z": "#1e90ff",
    "press": "#e1b12c", "vf": "#ffa502", "vr": "#00a8ff", "alt": "#e67e22",
}


class DebugPageMixin:
    """MainWindow 的 Debug 页（要求宿主有 self.page_debug）。"""

    _dbg_keys = ("gx", "gy", "gz", "ax", "ay", "az",
                 "mx", "my", "mz", "press", "temp", "vf", "vr", "alt", "qual",
                 "pn", "pe")

    # ------------------------------------------------------------ 搭页面
    def _build_debug_page(self):
        self._page_debug_idx = self.stackedWidget.indexOf(self.page_debug)

        box = self.page_debug.layout()
        box.setSpacing(8)

        self.dbg_info = QLabel("等待传感器数据…")
        self.dbg_info.setStyleSheet("font-size: 9pt; font-family: Consolas, monospace;")
        box.addWidget(self.dbg_info)

        self.dbg_data = {key: deque([0.0] * DATA_LEN, maxlen=DATA_LEN) for key in self._dbg_keys}
        self._press_ref = None          # 气压画相对值，否则 10 万 Pa 的绝对值看不出噪声

        self.dbg_gyr = self._make_dbg_plot("陀螺 °/s",         (-100.0, 100.0))
        self.dbg_acc = self._make_dbg_plot("加速度 m/s²",      (-20.0,  20.0))
        self.dbg_mag = self._make_dbg_plot("磁力计 LSB",       (-1000.0, 1000.0))
        self.dbg_bar = self._make_dbg_plot("相对气压 Pa")
        self.dbg_vel = self._make_dbg_plot("光流速度 m/s",     (-2.0,  2.0))
        self.dbg_alt = self._make_dbg_plot("光流高度 m")
        self.dbg_pos = self._make_dbg_plot("PVRS 估计位置 m")      # 北/东两条

        self.dbg_plots = (self.dbg_gyr, self.dbg_acc, self.dbg_mag,
                          self.dbg_bar, self.dbg_vel, self.dbg_alt, self.dbg_pos)

        self.dbg_curves = {
            "gx": self.dbg_gyr.plot(pen=pg.mkPen(DBG_COLOR["x"], width=2), name="Gx"),
            "gy": self.dbg_gyr.plot(pen=pg.mkPen(DBG_COLOR["y"], width=2), name="Gy"),
            "gz": self.dbg_gyr.plot(pen=pg.mkPen(DBG_COLOR["z"], width=2), name="Gz"),

            "ax": self.dbg_acc.plot(pen=pg.mkPen(DBG_COLOR["x"], width=2), name="Ax"),
            "ay": self.dbg_acc.plot(pen=pg.mkPen(DBG_COLOR["y"], width=2), name="Ay"),
            "az": self.dbg_acc.plot(pen=pg.mkPen(DBG_COLOR["z"], width=2), name="Az"),

            "mx": self.dbg_mag.plot(pen=pg.mkPen(DBG_COLOR["x"], width=2), name="Mx"),
            "my": self.dbg_mag.plot(pen=pg.mkPen(DBG_COLOR["y"], width=2), name="My"),
            "mz": self.dbg_mag.plot(pen=pg.mkPen(DBG_COLOR["z"], width=2), name="Mz"),

            "press": self.dbg_bar.plot(pen=pg.mkPen(DBG_COLOR["press"], width=2), name="气压"),

            "vf": self.dbg_vel.plot(pen=pg.mkPen(DBG_COLOR["vf"], width=2), name="前向"),
            "vr": self.dbg_vel.plot(pen=pg.mkPen(DBG_COLOR["vr"], width=2), name="右向"),

            "alt": self.dbg_alt.plot(pen=pg.mkPen(DBG_COLOR["alt"], width=2), name="高度"),

            "pn": self.dbg_pos.plot(pen=pg.mkPen(DBG_COLOR["y"], width=2), name="北"),
            "pe": self.dbg_pos.plot(pen=pg.mkPen(DBG_COLOR["z"], width=2), name="东"),
        }

        grid = QGridLayout()
        grid.setSpacing(8)
        grid.addWidget(self.dbg_gyr, 0, 0)
        grid.addWidget(self.dbg_acc, 0, 1)
        grid.addWidget(self.dbg_mag, 1, 0)
        grid.addWidget(self.dbg_bar, 1, 1)
        grid.addWidget(self.dbg_vel, 2, 0)
        grid.addWidget(self.dbg_alt, 2, 1)
        grid.addWidget(self.dbg_pos, 3, 0, 1, 2)          # 位置横跨两列

        box.addLayout(grid, 1)

        self.btn_debug.clicked.connect(lambda _, i=self._page_debug_idx: self._switch_page(i))

    @staticmethod
    def _make_dbg_plot(title: str, y_range=None):
        plot = pg.PlotWidget(title=title)
        plot.setXRange(0, DATA_LEN - 1, padding=0)
        plot.setMinimumHeight(DBG_PLOT_H)
        plot.showGrid(x=True, y=True, alpha=0.2)
        plot.addLegend(offset=(6, 4), labelTextSize="8pt")
        plot.setMouseEnabled(x=False, y=True)          # 纵轴还能自己滚轮缩放

        if y_range is None:
            plot.enableAutoRange(axis="y")
        else:
            plot.setYRange(y_range[0], y_range[1], padding=0)
            plot.disableAutoRange(axis="y")

        return plot

    # ------------------------------------------------------------ 收数据
    def _handle_dbg_line(self, line: str):
        parts = line.split(",")
        if len(parts) != DBG_FIELDS:
            return

        try:
            v = [int(item) for item in parts[1:]]
        except ValueError:
            return

        yaw, roll, pitch = v[0] / 10.0, v[1] / 10.0, v[2] / 10.0

        # 姿态和健康位也喂给主页面，实机就只用这一行遥测
        self.latest.update(yaw=yaw, roll=roll, pitch=pitch,
                           gyr=v[5], acc=v[6], mag=v[7], bar=v[8],
                           ofl=v[9], gps=v[10])
        self.yaw_data.append(yaw)
        self.roll_data.append(roll)
        self.pitch_data.append(pitch)

        values = {
            "gx": v[11] / 10.0, "gy": v[12] / 10.0, "gz": v[13] / 10.0,    # 0.1°/s
            "ax": v[14] / 100.0, "ay": v[15] / 100.0, "az": v[16] / 100.0,  # cm/s²
            "mx": float(v[17]), "my": float(v[18]), "mz": float(v[19]),     # LSB
            "press": float(v[20]),                                          # Pa
            "temp": v[21] / 10.0,                                           # 0.1℃
            "vf": v[22] / 1000.0, "vr": v[23] / 1000.0,                     # mm/s
            "alt": v[24] / 1000.0,                                          # mm
            "qual": float(v[26]),
            "pn": v[3] / 100.0, "pe": v[4] / 100.0,                         # cm
        }

        if self._press_ref is None:
            self._press_ref = values["press"]
        values["press"] -= self._press_ref          # 画相对值，十万帕的绝对值看不出噪声

        for key, value in values.items():
            self.dbg_data[key].append(value)

        self.dbg_flow_q = v[25]          # 光流置信度 0/100

    def _update_debug(self):
        if self.stackedWidget.currentIndex() != self._page_debug_idx:
            return

        for key, curve in self.dbg_curves.items():
            curve.setData(list(self.dbg_data[key]))

        self.dbg_info.setText("温度 %.1f ℃   激光质量 %d   光流 %d"
                              % (self.dbg_data["temp"][-1],
                                 self.dbg_data["qual"][-1],
                                 getattr(self, "dbg_flow_q", 0)))