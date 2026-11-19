import pyqtgraph as pg
from collections import deque
from config import DATA_LEN, LINK_ATTITUDE, LINK_IMU, LINK_OFL, LINK_STATUS
from PyQt5.QtWidgets import QGridLayout, QLabel

DBG_PLOT_H  = 150

DBG_COLOR = {
    "x": "#ff6b81", "y": "#2ed573", "z": "#1e90ff",
    "press": "#e1b12c", "vf": "#ffa502", "vr": "#00a8ff", "alt": "#e67e22",
}


class DebugPageMixin:
    """MainWindow 的 Debug 页（要求宿主有 self.page_debug）。"""

    _dbg_keys = ("gx", "gy", "gz", "ax", "ay", "az",
                 "mx", "my", "mz", "press", "temp", "vf", "vr", "alt", "qual")

    # ------------------------------------------------------------ 搭页面
    def _build_debug_page(self):
        self._page_debug_idx = self.stackedWidget.indexOf(self.page_debug)

        box = self.page_debug.layout()
        box.setSpacing(8)

        self.dbg_data = {key: deque([0.0] * DATA_LEN, maxlen=DATA_LEN) for key in self._dbg_keys}

        self.dbg_gyr = self._make_dbg_plot("陀螺 °/s",         (-100.0, 100.0))
        self.dbg_acc = self._make_dbg_plot("加速度 m/s²",      (-20.0,  20.0))
        self.dbg_mag = self._make_dbg_plot("磁力计 LSB",       (-1000.0, 1000.0))
        self.dbg_bar = self._make_dbg_plot("气压 kPa")
        self.dbg_vel = self._make_dbg_plot("光流速度 m/s",     (-2.0,  2.0))
        self.dbg_alt = self._make_dbg_plot("光流高度 m")

        self.dbg_plots = (self.dbg_gyr, self.dbg_acc, self.dbg_mag,
                          self.dbg_bar, self.dbg_vel, self.dbg_alt)

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
        }

        grid = QGridLayout()
        grid.setSpacing(8)
        grid.addWidget(self.dbg_gyr, 0, 0)
        grid.addWidget(self.dbg_acc, 0, 1)
        grid.addWidget(self.dbg_mag, 1, 0)
        grid.addWidget(self.dbg_bar, 1, 1)
        grid.addWidget(self.dbg_vel, 2, 0)
        grid.addWidget(self.dbg_alt, 2, 1)

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
    def _handle_link_frame(self, fid: int, v: list):
        """二进制帧进来后分派：姿态喂主页面，传感器喂本页曲线"""
        if fid == LINK_ATTITUDE:
            yaw, roll, pitch = v[0] / 10.0, v[1] / 10.0, v[2] / 10.0

            self.latest.update(yaw=yaw, roll=roll, pitch=pitch)
            self.yaw_data.append(yaw)
            self.roll_data.append(roll)
            self.pitch_data.append(pitch)

        elif fid == LINK_IMU:
            values = {
                "gx": v[0] / 10.0, "gy": v[1] / 10.0, "gz": v[2] / 10.0,      # 0.1°/s
                "ax": v[3] / 100.0, "ay": v[4] / 100.0, "az": v[5] / 100.0,   # cm/s²
                "mx": float(v[6]), "my": float(v[7]), "mz": float(v[8]),      # LSB
                "press": 90.0 + v[9] * 0.001,                                  # 气压 kPa（0.001kPa 偏移 90kPa）
                "temp": v[10] / 10.0,                                         # 0.1℃
            }

            for key, value in values.items():
                self.dbg_data[key].append(value)

        elif fid == LINK_OFL:
            for key, value in (("vf", v[0] / 1000.0), ("vr", v[1] / 1000.0),
                               ("alt", v[2] / 1000.0)):
                self.dbg_data[key].append(value)

            self.dbg_flow_q = v[3]
            self.dbg_data["qual"].append(float(v[4]))

        elif fid == LINK_STATUS:
            self.latest.update(gyr=v[0], acc=v[1], mag=v[2], bar=v[3],
                               ofl=v[4], gps=v[5])

    def _update_debug(self):
        if self.stackedWidget.currentIndex() != self._page_debug_idx:
            return

        for key, curve in self.dbg_curves.items():
            curve.setData(list(self.dbg_data[key]))