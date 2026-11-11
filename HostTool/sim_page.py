"""仿真页：虚拟摇杆 + 姿态球 + 四张曲线 + 解锁按钮。

所有控制律都在固件里跑，这里只做三件事：
    1. 50Hz 把两个摇杆的杆量发下去（rc 命令）
    2. 收 50Hz 的 SIM 遥测，画曲线、刷新读数
    3. 解锁按钮（arm 命令），状态以固件回传为准
"""

import pyqtgraph as pg
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QGridLayout, QHBoxLayout, QLabel, QPushButton, QSizePolicy, QVBoxLayout,
)

from attitude_indicator import AttitudeIndicator
from config import (
    DATA_LEN, PAGE_SIM, RC_KEY_STEP, SIM_COLOR, SIM_FIELDS, SIM_IDX_E,
    SIM_IDX_M1, SIM_IDX_N, SIM_MISS_MAX, SIM_PLOT_H,
)
from virtual_map import VirtualMap
from widgets import StickPad


class SimPageMixin:
    """MainWindow 的仿真页部分（要求宿主有 self.page_config / self.stackedWidget）。"""

    # ------------------------------------------------------------ 搭界面
    def _build_simulation(self):
        self.btn_config.setText("Simu")

        layout = QVBoxLayout(self.page_config)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        layout.addLayout(self._build_sim_row())
        layout.addLayout(self._build_sim_body(), 1)

        # 曲线加完之后再锁一次横轴：addCurve 会把自动量程重新打开
        for plot, y_min, y_max in self.sim_plots:
            plot.setYRange(y_min, y_max, padding=0)
            plot.setXRange(0, DATA_LEN - 1, padding=0)
            plot.setMinimumHeight(SIM_PLOT_H)
            plot.setMaximumHeight(200)          # 别把下面的地图挤没了

    def _build_sim_row(self):
        """顶部一行：解锁按钮 + 数字读数，整体居中"""
        self.arm_button = QPushButton("未解锁")
        self.arm_button.setCheckable(True)
        self.arm_button.setFixedSize(84, 26)
        self.arm_button.clicked.connect(self._on_arm_clicked)
        self.arm_button.setEnabled(False)          # 没连接之前不许切换

        self.sim_numbers = QLabel("高度  --cm    油门  --‰    姿态  --/--    位置  北 --  东 --")
        self.sim_numbers.setStyleSheet("font-size: 9pt; font-family: Consolas, monospace;")

        row = QHBoxLayout()
        row.setSpacing(12)
        row.addStretch(1)
        row.addWidget(self.arm_button)
        row.addWidget(self.sim_numbers)              # 高度/油门/姿态/位置合成一行
        row.addStretch(1)

        return row

    def _build_sim_body(self):
        """左边摇杆+姿态球，右边四张图"""
        # 美国手（Mode 2）：左杆高度+偏航，右杆俯仰+横滚，都自动回中
        self.stick_left  = StickPad("1000", "0", "−100", "+100",
                                    spring_x=True, spring_y=True)
        self.stick_right = StickPad("+100", "−100", "−100", "+100",
                                    spring_x=True, spring_y=True)
        self.stick_left.y = 0.0                    # 初始居中 = 定高

        sticks = QHBoxLayout()
        sticks.setSpacing(10)
        sticks.addLayout(self._stick_column("左摇杆", self.stick_left))
        sticks.addLayout(self._stick_column("右摇杆", self.stick_right))
        sticks.addStretch(1)

        self.sim_ahi = AttitudeIndicator()
        self.sim_ahi.setFixedWidth(310)            # 和两个摇杆的总宽对齐
        self.sim_ahi.setMinimumHeight(310)
        self.sim_ahi.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Expanding)
        self.sim_ahi.align_top = True              # 圆盘贴顶，底色铺到底

        left_col = QVBoxLayout()
        left_col.setSpacing(6)
        left_col.addLayout(sticks)
        left_col.addWidget(self.sim_ahi, 1)

        self.sim_att_plot = self._make_sim_plot("姿态",   -180,  180)
        self.sim_gyr_plot = self._make_sim_plot("角速度", -250,  250)
        self.sim_thr_plot = self._make_sim_plot("油门",      0, 1000)
        self.sim_alt_plot = self._make_sim_plot("高度",      0, 1000)

        self.sim_plots = (
            (self.sim_att_plot, -180, 180),
            (self.sim_gyr_plot, -250, 250),
            (self.sim_thr_plot,    0, 1000),
            (self.sim_alt_plot,    0, 1000),
        )

        grid = QGridLayout()
        grid.setSpacing(8)
        grid.addWidget(self.sim_att_plot, 0, 0)
        grid.addWidget(self.sim_gyr_plot, 0, 1)
        grid.addWidget(self.sim_thr_plot, 1, 0)
        grid.addWidget(self.sim_alt_plot, 1, 1)

        right = QVBoxLayout()
        right.setSpacing(8)
        right.addLayout(grid)                           # 四张曲线

        self.vmap = VirtualMap()                        # 虚拟地图：不用联网
        right.addWidget(self.vmap, 1)

        body = QHBoxLayout()
        body.setSpacing(10)
        body.addLayout(left_col)
        body.addLayout(right, 1)

        self._build_sim_curves()

        return body

    @staticmethod
    def _stick_column(caption: str, pad: StickPad):
        box = QVBoxLayout()
        box.setSpacing(0)
        label = QLabel(caption)
        label.setAlignment(Qt.AlignCenter)
        label.setStyleSheet("font-size: 9pt;")
        box.addWidget(label)
        box.addWidget(pad)
        return box

    def _build_sim_curves(self):
        att, gyr, thr, alt = self.sim_att_plot, self.sim_gyr_plot, self.sim_thr_plot, self.sim_alt_plot

        self.sim_curve = {
            "roll":  att.plot(pen=pg.mkPen(SIM_COLOR["roll"],  width=2), name="Roll"),
            "pitch": att.plot(pen=pg.mkPen(SIM_COLOR["pitch"], width=2), name="Pitch"),
            "yaw":   att.plot(pen=pg.mkPen(SIM_COLOR["yaw"],   width=2), name="Yaw"),

            "gx":    gyr.plot(pen=pg.mkPen(SIM_COLOR["gx"], width=2), name="Gx"),
            "gy":    gyr.plot(pen=pg.mkPen(SIM_COLOR["gy"], width=2), name="Gy"),
            "gz":    gyr.plot(pen=pg.mkPen(SIM_COLOR["gz"], width=2), name="Gz"),

            "m1":    thr.plot(pen=pg.mkPen(SIM_COLOR["m1"], width=2), name="M1"),
            "m2":    thr.plot(pen=pg.mkPen(SIM_COLOR["m2"], width=2), name="M2"),
            "m3":    thr.plot(pen=pg.mkPen(SIM_COLOR["m3"], width=2), name="M3"),
            "m4":    thr.plot(pen=pg.mkPen(SIM_COLOR["m4"], width=2), name="M4"),

            "alt":   alt.plot(pen=pg.mkPen(SIM_COLOR["alt"], width=2), name="高度"),
        }

    def _make_sim_plot(self, title: str, y_min: int, y_max: int):
        plot = pg.PlotWidget(title=title)
        plot.setYRange(y_min, y_max)
        plot.setXRange(0, DATA_LEN - 1)                       # 横轴 = 采样点序号
        plot.showGrid(x=True, y=True, alpha=0.2)
        plot.addLegend(offset=(6, 4), labelTextSize="8pt")
        plot.setMouseEnabled(x=False, y=False)
        return plot

    # ------------------------------------------------------------ 收数据
    def _handle_sim_line(self, line: str):
        """解析一行 SIM 数据：SIM,roll,pitch,yaw,gx,gy,gz,m1..m4,alt,arm（×10 的整数）"""
        parts = line.split(",")
        if len(parts) != SIM_FIELDS:
            return
        try:
            data = {
                "roll":  int(parts[1]) / 10.0,
                "pitch": int(parts[2]) / 10.0,
                "yaw":   int(parts[3]) / 10.0,
                "gx":    int(parts[4]) / 10.0,
                "gy":    int(parts[5]) / 10.0,
                "gz":    int(parts[6]) / 10.0,
                "m1":    int(parts[SIM_IDX_M1]),
                "m2":    int(parts[SIM_IDX_M1 + 1]),
                "m3":    int(parts[SIM_IDX_M1 + 2]),
                "m4":    int(parts[SIM_IDX_M1 + 3]),
                "alt":   int(parts[11]),
                "arm":   int(parts[12]),
                "n":     int(parts[SIM_IDX_N]) / 100.0,
                "e":     int(parts[SIM_IDX_E]) / 100.0,
            }
        except (ValueError, IndexError):
            return

        self.sim_latest.update(data)
        for key in SIM_COLOR:
            self.sim_curves[key].append(float(data[key]))

        self._sync_arm_button(data["arm"] != 0)

        self.sim_seq += 1

    def _update_sim(self):
        if self.sim_seq != self.sim_seen:
            self.sim_seen = self.sim_seq
            self.sim_miss = 0
        elif self.sim_miss < SIM_MISS_MAX:
            self.sim_miss += 1

        if self.sim_miss >= SIM_MISS_MAX:
            if self.sim_miss == SIM_MISS_MAX:
                self.sim_numbers.setText("无数据 —— 1 秒没收到 SIM 帧")
            return                      # 掉线就不刷了

        if self.stackedWidget.currentIndex() != PAGE_SIM:
            return                      # 不在仿真页就不用刷曲线

        d = self.sim_latest

        self.sim_numbers.setText("高度 %4d cm   油门 %4d ‰   姿态 %+3d / %+3d   位置 北 %+6.1f  东 %+6.1f m"
                                 % (d["alt"], d["m1"], d["roll"], d["pitch"], d["n"], d["e"]))

        self.vmap.set_position(d["n"], d["e"], d["yaw"], d["alt"] / 100.0, True)

        self.sim_ahi.set_attitude(d["roll"], d["pitch"], d["yaw"])

        for key, curve in self.sim_curve.items():
            curve.setData(list(self.sim_curves[key]))

    # ------------------------------------------------------------ 发数据
    def _send_rc(self):
        """50Hz 把两个摇杆的杆量灌给固件（虚拟遥控）"""
        Thro  = (100.0 - self.stick_left.y) * 5.0      # 杆上推 = 油门大，0~1000
        Yaw   = self.stick_left.x
        Roll  = self.stick_right.x
        Pitch = self.stick_right.y

        if self.serial_thread is None or self.serial_thread.ser is None:
            return

        try:
            self.serial_thread.ser.write(
                f"rc,{Thro:.0f},{Yaw:.0f},{Roll:.0f},{Pitch:.0f}\n".encode())
        except Exception:
            pass

    # ------------------------------------------------------------ 解锁
    def _on_arm_clicked(self):
        """解锁/上锁按钮：点一下发一次 arm,<0|1>，固件状态从遥测回来"""
        Arm = 1 if self.arm_button.isChecked() else 0

        if self.serial_thread is None or self.serial_thread.ser is None:
            self.arm_button.setChecked(not Arm)     # 没连接：撤销这次切换
            return

        self.arm_button.setText("已解锁" if Arm else "未解锁")
        self._style_arm_button()

        try:
            self.serial_thread.ser.write(f"arm,{Arm}\n".encode())
        except Exception:
            self.arm_button.setChecked(not Arm)

    def _sync_arm_button(self, Arm: bool):
        """按钮状态跟着固件走：固件解锁才显示已解锁，点错了立刻看得出来"""
        if self.arm_button.isChecked() == Arm:
            return

        self.arm_button.setChecked(Arm)
        self.arm_button.setText("已解锁" if Arm else "未解锁")
        self._style_arm_button()

    def _style_arm_button(self):
        """解锁按钮配色：已解锁=绿，未解锁=红（跟随主题）"""
        if self.current_theme == "dark":
            bg, border, fg = (("#17351f", "#2f7a4a", "#86efac") if self.arm_button.isChecked()
                              else ("#3a1d1d", "#7a3535", "#fca5a5"))
            off_bg, off_fg = "#1a2332", "#55606e"
        else:
            bg, border, fg = (("#e8f8ee", "#8fd3a6", "#15803d") if self.arm_button.isChecked()
                              else ("#fdecec", "#e8a0a0", "#b91c1c"))
            off_bg, off_fg = "#f0f2f5", "#9aa4b0"

        self.arm_button.setStyleSheet(f"""
            QPushButton {{
                color: {fg};
                background-color: {bg};
                border: 1px solid {border};
                border-radius: 6px;
                font-size: 9pt;
            }}
            QPushButton:hover {{ border: 1px solid {fg}; }}
            QPushButton:disabled {{
                color: {off_fg};
                background-color: {off_bg};
                border: 1px solid {off_bg};
            }}
        """)

    # ------------------------------------------------------------ 键盘
    def _key_step(self):
        if not self.keys_down:
            return

        if self.stackedWidget.currentIndex() != PAGE_SIM:
            return                      # 别的页面敲键不动摇杆

        left  = self.stick_left
        right = self.stick_right
        moved = False

        if Qt.Key_W in self.keys_down:
            left.y = max(-100.0, left.y - RC_KEY_STEP); moved = True
        if Qt.Key_S in self.keys_down:
            left.y = min(100.0, left.y + RC_KEY_STEP); moved = True
        if Qt.Key_A in self.keys_down:
            left.x = max(-100.0, left.x - RC_KEY_STEP); moved = True
        if Qt.Key_D in self.keys_down:
            left.x = min(100.0, left.x + RC_KEY_STEP); moved = True

        if Qt.Key_Up in self.keys_down:                 # ↑ 低头前飞
            right.y = max(-100.0, right.y - RC_KEY_STEP); moved = True
        if Qt.Key_Down in self.keys_down:               # ↓ 抬头
            right.y = min(100.0, right.y + RC_KEY_STEP); moved = True
        if Qt.Key_Left in self.keys_down:
            right.x = max(-100.0, right.x - RC_KEY_STEP); moved = True
        if Qt.Key_Right in self.keys_down:
            right.x = min(100.0, right.x + RC_KEY_STEP); moved = True

        if moved:
            left.update()
            right.update()

    def _release_pad(self, key):
        """松键回中：四个方向各归各的轴"""
        pads = (
            (Qt.Key_W,       self.stick_left,  "y"),
            (Qt.Key_S,       self.stick_left,  "y"),
            (Qt.Key_A,       self.stick_left,  "x"),
            (Qt.Key_D,       self.stick_left,  "x"),
            (Qt.Key_Up,      self.stick_right, "y"),
            (Qt.Key_Down,    self.stick_right, "y"),
            (Qt.Key_Left,    self.stick_right, "x"),
            (Qt.Key_Right,   self.stick_right, "x"),
        )

        for k, pad, axis in pads:
            if k == key:
                setattr(pad, axis, 0.0)
                pad.update()
                return True

        return False