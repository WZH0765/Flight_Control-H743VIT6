import sys
import os
from collections import deque

import serial.tools.list_ports
import pyqtgraph as pg
from PyQt5.QtCore import Qt, QTimer, QSettings, QEvent
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, QMessageBox,
    QGridLayout, QLabel, QWidget,
)
from PyQt5 import uic

from attitude_indicator import AttitudeIndicator
from config import (
    DATA_LEN, DEFAULT_BAUD, IDX_ACC, IDX_BAR, IDX_GYR, IDX_MAG, IDX_PITCH,
    IDX_ROLL, IDX_YAW, PAGE_ABOUT, PAGE_CALIBRATION, PAGE_DASHBOARD, PAGE_HELP,
    PAGE_SIM, PROTOCOL_FIELDS, RC_SEND_MS, REFRESH_MS, SIM_COLOR, SIM_MISS_MAX,
)
from help_text import HELP_DOCS, HELP_TOPICS
from serial_thread import SerialThread
from sim_page import SimPageMixin
from theme import THEMES, button_style, combo_style, global_style
from widgets import FACE_TEXT, FaceTile, FACE_COLOR

# ---------------------------------------------------------------- 常量
pg.setConfigOptions(antialias=True)

QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

def resource_path(relative_path: str) -> str:
    """获取资源绝对路径（兼容 PyInstaller 打包）"""
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), relative_path)

UI_PATH = resource_path("FC Configurator.ui")

# ---------------------------------------------------------------- 主窗口
class MainWindow(SimPageMixin, QMainWindow):

    def __init__(self):
        super().__init__()
        uic.loadUi(UI_PATH, self)
        self._set_window_icon()

        # .ui 里带着一个空状态栏，深色主题下会在底部留一条白边；程序没用到它，直接藏掉
        self.statusbar.hide()
        self.menubar.hide()

        # --- 运行时状态 ---
        self.serial_thread = None
        self.latest = {
            "yaw": 0.0, "roll": 0.0, "pitch": 0.0,
            "gyr": 1, "acc": 1, "mag": 1, "bar": 1,
        }

        self.yaw_data   = deque([0] * DATA_LEN, maxlen=DATA_LEN)
        self.roll_data  = deque([0] * DATA_LEN, maxlen=DATA_LEN)
        self.pitch_data = deque([0] * DATA_LEN, maxlen=DATA_LEN)

        # --- 仿真数据 ---
        self.sim_latest = {
            "t": 0, "roll": 0.0, "pitch": 0.0, "yaw": 0.0,
            "gx": 0.0, "gy": 0.0, "gz": 0.0,
            "m1": 0, "m2": 0, "m3": 0, "m4": 0, "alt": 0.0,
        }
        self.sim_curves = {
            key: deque([0.0] * DATA_LEN, maxlen=DATA_LEN)
            for key in SIM_COLOR
        }
        self.sim_seq  = 0         # 收到的仿真帧数
        self.sim_seen = 0         # 上一帧刷新时看到的帧数
        self.sim_miss = SIM_MISS_MAX

        # --- UI 组装 ---
        self._build_plot()
        self._build_attitude_indicator()
        self._build_simulation()

        # --- 定时刷新 ---
        self.refresh_timer = QTimer(self)
        self.refresh_timer.setInterval(REFRESH_MS)
        self.refresh_timer.timeout.connect(self.refresh_ui)

        # --- 虚拟遥控发包 ---
        self.rc_timer = QTimer(self)
        self.rc_timer.setInterval(RC_SEND_MS)
        self.rc_timer.timeout.connect(self._send_rc)
        self.rc_timer.start()

        # --- 键盘控制左杆（W/S 油门，A/D 偏航）---
        self.keys_down = set()

        self.key_timer = QTimer(self)
        self.key_timer.setInterval(20)
        self.key_timer.timeout.connect(self._key_step)
        self.key_timer.start()

        QApplication.instance().installEventFilter(self)

        # --- 主题 ---
        self.settings = QSettings("FlightControl", "HostTool")
        self.current_theme = self.settings.value("theme", "light")

        # --- 信号绑定与初始化 ---
        self._bind_signals()
        self._init_navigation()
        self._init_calibration()
        self._init_help_content()

        # --- 其他初始化 ---
        self.refresh_ports()
        self.apply_theme(self.current_theme)

    # ============================================================ 初始化
    def _set_window_icon(self):
        icon_path = resource_path("icon.ico")
        if os.path.exists(icon_path):
            self.setWindowIcon(QIcon(icon_path))

    def _build_plot(self):
        self.plot = pg.PlotWidget()
        self.plot.setYRange(-180, 180)
        self.plot.showGrid(x=True, y=True, alpha=0.2)
        self.plot.addLegend(offset=(10, 10))

        self.curve_yaw   = self.plot.plot(pen=pg.mkPen("#ff4757", width=2), name="Yaw")
        self.curve_roll  = self.plot.plot(pen=pg.mkPen("#2ed573", width=2), name="Roll")
        self.curve_pitch = self.plot.plot(pen=pg.mkPen("#a55eea", width=2), name="Pitch")

        layout = QVBoxLayout(self.plot_frame)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.plot)

        # 提示信息：绘图区右上角悬浮
        self.hint_label = QLabel("", self.plot_frame)
        self.plot_frame.installEventFilter(self)

    def _build_attitude_indicator(self):
        self.ahi = AttitudeIndicator()
        layout = QVBoxLayout(self.attitude_frame)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.ahi)

    # ============================================================ 提示信息
    def _show_hint(self, text):
        self.hint_label.setText(text)
        self._place_hint()

    def _place_hint(self):
        self.hint_label.adjustSize()
        self.hint_label.move(self.plot_frame.width() - self.hint_label.width() - 8, 8)
        self.hint_label.raise_()

    def eventFilter(self, obj, event):
        if obj is self.plot_frame and event.type() == QEvent.Resize:
            self._place_hint()

        # 仿真页吃掉 WASD 和方向键，驱动两个摇杆（其他页面不拦，免得影响别处输入）
        if event.type() in (QEvent.KeyPress, QEvent.KeyRelease) and not event.isAutoRepeat():
            if self.stackedWidget.currentIndex() == PAGE_SIM:
                if event.type() == QEvent.KeyPress:
                    self.keys_down.add(event.key())
                    return True

                if event.key() in self.keys_down:
                    self.keys_down.discard(event.key())
                    self._release_pad(event.key())     # 松键回中
                    return True

        return super().eventFilter(obj, event)

    def _bind_signals(self):
        self.theme_button.clicked.connect(self.toggle_theme)
        self.refresh_button.clicked.connect(self.refresh_ports)
        self.link_button.clicked.connect(self.toggle_connection)

    # ============================================================ 导航
    def _init_navigation(self):
        self._nav_map = [
            (self.btn_dashboard,   PAGE_DASHBOARD),
            (self.btn_calibration, PAGE_CALIBRATION),
            (self.btn_config,      PAGE_SIM),
            (self.btn_help,        PAGE_HELP),
            (self.btn_about,       PAGE_ABOUT),
        ]
        for btn, idx in self._nav_map:
            btn.clicked.connect(lambda _, i=idx: self._switch_page(i))

        self._switch_page(PAGE_DASHBOARD)

    def _switch_page(self, index: int):
        self.stackedWidget.setCurrentIndex(index)
        
        t = THEMES[self.current_theme]
        active_bg = t.get('nav_checked_bg', '#0078d4')
        active_fg = t.get('nav_checked_fg', '#ffffff')
        normal_fg = t.get('nav_button', '#333333')
        hover_bg  = t.get('nav_hover', '#e8eef5')

        for btn, idx in self._nav_map:
            if idx == index:
                btn.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {active_bg};
                        color: {active_fg};
                        border: 1px solid {active_bg};
                        border-radius: 4px;
                        font-weight: bold;
                    }}
                """)
            else:
                btn.setStyleSheet(f"""
                    QPushButton {{
                        background-color: transparent;
                        color: {normal_fg};
                        border: 1px solid transparent;
                        border-radius: 4px;
                    }}
                    QPushButton:hover {{
                        background-color: {hover_bg};
                    }}
                """)

    # ============================================================ Help 页面
    def _init_help_content(self):
        self.help_list.addItems(HELP_TOPICS)

        self._help_docs = HELP_DOCS

        self.help_list.currentTextChanged.connect(self._on_help_topic_changed)
        self.help_list.setCurrentRow(0)

    def _on_help_topic_changed(self, topic: str):
        if topic in self._help_docs:
            self.help_text.setHtml(self._help_docs[topic])

    # ============================================================ 校准
    def _init_calibration(self):
        # 气压计不需要校准，隐藏原来的 BAR 列
        for widget in (self.bar_label, self.bar_note,
                       self.start_bar, self.reset_bar, self.progressBar_bar):
            widget.hide()

        self.accgyr_label.setText("IMU")
        self.accgyr_note.setText("按图示摆放\n放稳后点 NEXT")
        self.reset_accgyr.setText("STOP")
        self.reset_mag.setText("STOP")

        self.face_tiles = {}
        self.face_panel = QWidget()
        face_layout = QGridLayout(self.face_panel)
        face_layout.setContentsMargins(0, 0, 0, 0)
        face_layout.setSpacing(6)
        for face in range(1, 7):
            tile = FaceTile(face)
            self.face_tiles[face] = tile
            face_layout.addWidget(tile, (face - 1) // 3, (face - 1) % 3)
        self.gridLayout.addWidget(self.face_panel, 0, 2, 2, 8)

        self.gridLayout.removeWidget(self.mag_note)
        self.gridLayout.addWidget(self.mag_note, 2, 1, 1, 9)

        self.start_accgyr.clicked.connect(self._on_imu_button)
        self.start_mag.clicked.connect(   lambda: self._send_cal_cmd("mag"))
        self._imu_button("Start", True)

        self.reset_accgyr.clicked.connect(lambda: self._abort_cal("imu"))
        self.reset_mag.clicked.connect(   lambda: self._abort_cal("mag"))

        for pb in (self.progressBar_accgyr, self.progressBar_mag):
            self._set_busy(pb, False)
            pb.setValue(0)

        self._append_cal_log("[INFO] IMU / MAG 校准就绪")
        self._append_cal_log("[INFO] IMU:先放好 FACE1 再点 Start,之后每面放好点 NEXT")

    def _cal_progress(self, sensor: str):
        return self.progressBar_accgyr if sensor == "imu" else self.progressBar_mag

    def _reset_faces(self):
        for tile in self.face_tiles.values():
            tile.set_state("idle")

    def _set_face_stage(self, face: int):
        for n, tile in self.face_tiles.items():
            if n < face:
                tile.set_state("done")
            elif n == face:
                tile.set_state("active")
            else:
                tile.set_state("idle")

    def _finish_faces(self):
        for tile in self.face_tiles.values():
            tile.set_state("done")

    @staticmethod
    def _set_busy(progress_bar, busy: bool):
        if busy:
            progress_bar.setRange(0, 0)
        else:
            progress_bar.setRange(0, 100)

    def _on_imu_button(self):
        if self.start_accgyr.text() == "NEXT":
            self._send_cal_next()
        else:
            self._send_cal_cmd("imu")

    def _imu_button(self, text: str, enabled: bool = True):
        self.start_accgyr.setText(text)
        self.start_accgyr.setEnabled(enabled)

    def _send_cmd(self, cmd: str):
        if self.serial_thread is None or self.serial_thread.ser is None:
            self.show_warning("提示", "请先连接串口")
            return False

        try:
            self.serial_thread.ser.write(cmd.encode())
            self._append_cal_log(f"[TX] {cmd.strip()}")
            return True
        except Exception as e:
            self.show_warning("发送失败", str(e))
            return False

    def _send_cal_cmd(self, sensor: str):
        if not self._send_cmd(f"cal,{sensor},start\n"):
            return

        progress_bar = self._cal_progress(sensor)
        self._set_busy(progress_bar, False)
        progress_bar.setValue(0)

        if sensor == "imu":
            self._imu_button("Start", False)
            self._reset_faces()

    def _send_cal_next(self):
        if not self._send_cmd("cal,imu,next\n"):
            return

        self._imu_button("Start", False)

    def _abort_cal(self, sensor: str):
        self._send_cmd(f"cal,{sensor},abort\n")

        progress_bar = self._cal_progress(sensor)
        self._set_busy(progress_bar, False)
        progress_bar.setValue(0)

        if sensor == "imu":
            self._imu_button("Start", True)
            self._reset_faces()

        self._append_cal_log(f"[INFO] {sensor.upper()} 校准已停止")

    def _handle_cal_line(self, line: str):
        self._append_cal_log(f"[RX] {line}")

        parts = line.split(",")
        if len(parts) < 3:
            return

        sensor = parts[1].strip().lower()
        state  = parts[2].strip().upper()
        detail = parts[3].strip().upper() if len(parts) > 3 else ""

        name = sensor.upper()
        progress_bar = self._cal_progress(sensor)

        if state == "CHECK":
            self._set_busy(progress_bar, False)
            progress_bar.setValue(0)
            self._append_cal_log(f"[INFO] 请将 FACE1：{FACE_TEXT[1]} 放好，保持静止...")
            if sensor == "imu":
                self._imu_button("Start", False)
                self._reset_faces()
                self.face_tiles[1].set_state("active")

        elif state == "COLLECT":
            if sensor == "imu":
                self._imu_button("Start", False)
                self._set_busy(progress_bar, False)
                if detail.startswith("FACE"):
                    face = int(detail[4:]) if detail[4:].isdigit() else 0
                    progress_bar.setValue(min(90, max(0, (face - 1) * 15)))
                    self._append_cal_log(f"[INFO] 正在采集 FACE{face}...")
                    self._set_face_stage(face)
                elif detail == "GYRO":
                    progress_bar.setValue(90)
                    self._append_cal_log("[INFO] 正在采集陀螺零偏...")
                    self._finish_faces()
            else:
                self._set_busy(progress_bar, True)
                self._append_cal_log("[INFO] 请缓慢翻转板子，覆盖各个方向（约 10 秒）")

        elif state == "WAIT":
            self._set_busy(progress_bar, False)
            if detail == "GYRO":
                progress_bar.setValue(90)
                self._append_cal_log("[INFO] 请保持静止，然后点击 NEXT 采集陀螺")
                self._finish_faces()
            elif detail.startswith("FACE"):
                face = int(detail[4:]) if detail[4:].isdigit() else 0
                progress_bar.setValue(min(90, max(0, (face - 1) * 15)))
                self._append_cal_log(f"[INFO] 请将 {FACE_TEXT.get(face, detail)}，然后点击 NEXT")
                self._set_face_stage(face)
            if sensor == "imu":
                self._imu_button("NEXT", True)

        elif state == "SCALE":
            self._append_cal_log(f"[INFO] Acc_Scal = {detail}")

        elif state == "SAVE":
            self._set_busy(progress_bar, False)
            progress_bar.setValue(0)
            self._append_cal_log(f"[WARN] {name} 校准完成，但 Flash 保存失败，已标记未校准")
            if sensor == "imu":
                self._imu_button("Start", True)
                self._reset_faces()

        elif state == "DONE":
            self._set_busy(progress_bar, False)
            progress_bar.setValue(100)
            self._append_cal_log(f"[INFO] {name} 校准完成")
            if sensor == "imu":
                self._imu_button("Start", True)
                self._finish_faces()

        elif state == "ERROR":
            self._set_busy(progress_bar, False)
            progress_bar.setValue(0)
            self._append_cal_log(f"[ERROR] {name} 校准失败")
            if sensor == "imu":
                self._imu_button("Start", True)
                self._reset_faces()

        elif state == "IDLE":
            self._set_busy(progress_bar, False)
            progress_bar.setValue(0)
            if detail == "ABORTED":
                self._append_cal_log(f"[INFO] {name} 校准已取消")
            if sensor == "imu":
                self._imu_button("Start", True)
                self._reset_faces()

        elif state == "BUSY":
            self._append_cal_log(f"[WARN] {name} 正在校准中")
            if sensor == "imu":
                self._imu_button("Start", True)

    def _append_cal_log(self, text: str):
        self.calib_text.appendPlainText(text)

    # ============================================================ 主题
    def apply_theme(self, theme_name: str):
        t = THEMES[theme_name]
        self.current_theme = theme_name

        self.centralwidget.setStyleSheet(f"background-color: {t['centralwidget']};")
        self.setStyleSheet(global_style(theme_name, FACE_COLOR))

        normal_btns = [
            self.theme_button, self.refresh_button, self.link_button,
            self.start_accgyr, self.start_mag,
            self.reset_accgyr, self.reset_mag
        ]

        for btn in normal_btns:
            btn.setStyleSheet(button_style(theme_name))
        self.port_box.setStyleSheet(combo_style(theme_name))
        self._style_arm_button()

        # 仿真页的读数跟着主题走
        self.sim_numbers.setStyleSheet(
            "font-size: 9pt; font-family: Consolas, monospace; color: %s;" % t["label_color"])

        # 曲线和姿态球
        self.plot.setBackground(t["plot_bg"])
        for ax_name in ("left", "bottom"):
            axis = self.plot.getAxis(ax_name)
            axis.setPen(t["plot_grid"])
            axis.setTextPen(t["plot_text"])

        # 仿真页四张图跟着主题走（背景、轴线、标题、图例文字）
        for plot, _y_min, _y_max in self.sim_plots:
            plot.setBackground(t["plot_bg"])
            for ax_name in ("left", "bottom"):
                axis = plot.getAxis(ax_name)
                axis.setPen(t["plot_grid"])
                axis.setTextPen(t["plot_text"])

            plot.setTitle(plot.plotItem.titleLabel.text, color=t["plot_text"])
            if plot.plotItem.legend is not None:
                plot.plotItem.legend.setLabelTextColor(t["plot_text"])

        self.ahi.set_colors(bg=t["plot_bg"], border=t["plot_grid"], text=t["plot_text"])

        # 仿真页：姿态球配色 + 摇杆底色跟主题
        self.sim_ahi.set_colors(bg=t["plot_bg"], border=t["plot_grid"], text=t["plot_text"])
        for pad in (self.stick_left, self.stick_right):
            pad.set_theme(t["plot_bg"], t["plot_grid"], t["plot_text"])

        self.theme_button.setText("☀" if theme_name == "light" else "🌙")

        # 刷新导航按钮颜色
        self._switch_page(self.stackedWidget.currentIndex())

    def toggle_theme(self):
        new_theme = "dark" if self.current_theme == "light" else "light"
        self.apply_theme(new_theme)
        self.settings.setValue("theme", new_theme)

    # ============================================================ 弹窗
    def show_warning(self, title: str, text: str):
        msg = QMessageBox(self)
        msg.setIcon(QMessageBox.Warning)
        msg.setWindowTitle(title)
        msg.setText(text)

        if self.current_theme == "dark":
            msg.setStyleSheet("""
                QMessageBox              { background-color: #1a2332; }
                QMessageBox QLabel       { color: #e8eaed; }
                QMessageBox QPushButton  {
                    color: #e8eaed;
                    background-color: #2a3a4a;
                    border: 1px solid #3a4a5a;
                    border-radius: 4px;
                    padding: 6px 20px;
                    min-width: 60px;
                }
                QMessageBox QPushButton:hover { background-color: #3a4a5a; }
            """)
        msg.exec_()

    # ============================================================ 串口
    def refresh_ports(self):
        self.port_box.clear()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            self.port_box.addItems(ports)

    def toggle_connection(self):
        if self.serial_thread is None:
            self._connect()
        else:
            self._disconnect()

    def _connect(self):
        port = self.port_box.currentText().strip()
        if not port:
            self.show_warning("提示", "请先选择串口")
            return

        self.serial_thread = SerialThread(port, baud=DEFAULT_BAUD)
        self.serial_thread.line_received.connect(self.on_data)
        self.serial_thread.connection_result.connect(self.on_connection_result)
        self.serial_thread.start()

    def _disconnect(self):
        if self.serial_thread is not None:
            self.serial_thread.stop()
            self.serial_thread = None
        self.refresh_timer.stop()
        self.link_button.setText("CONNECT")
        self.link_status.setText("NOT CONNECTED")

        # 断开后按钮回到"未解锁"并禁用，防止离线状态下瞎切换
        self.arm_button.setChecked(False)
        self.arm_button.setText("未解锁")
        self.arm_button.setEnabled(False)
        self._style_arm_button()

        self._show_hint("")

        for pb in (self.progressBar_accgyr, self.progressBar_mag):
            self._set_busy(pb, False)
            pb.setValue(0)

        self._imu_button("Start", True)

    def on_connection_result(self, success: bool, error_msg: str):
        if success:
            self.link_button.setText("DISCONNECT")
            self.link_status.setText("CONNECTED")
            self.refresh_timer.start()

            # 连上了才允许切换解锁；状态以固件遥测为准
            self.arm_button.setChecked(False)
            self.arm_button.setText("未解锁")
            self.arm_button.setEnabled(True)
            self._style_arm_button()

            self._show_hint("校准参数：查询中…")
            try:
                self.serial_thread.ser.write(b"cal,status\n")
            except Exception:
                pass
        else:
            self.serial_thread = None
            self.link_button.setText("CONNECT")
            self.link_status.setText("NOT CONNECTED")
            self.show_warning(
                "连接失败",
                f"无法打开 {self.port_box.currentText()}\n\n{error_msg}",
            )

    # ============================================================ 数据
    def _parse_line(self, line: str):
        parts = line.split(",")
        if len(parts) != PROTOCOL_FIELDS:
            return None
        try:
            return {
                "yaw":   float(parts[IDX_YAW]),
                "roll":  float(parts[IDX_ROLL]),
                "pitch": float(parts[IDX_PITCH]),
                "gyr":   int(parts[IDX_GYR]),
                "acc":   int(parts[IDX_ACC]),
                "mag":   int(parts[IDX_MAG]),
                "bar":   int(parts[IDX_BAR]),
            }
        except (ValueError, IndexError):
            return None

    def on_data(self, line: str):
        upper = line.upper()
        if upper.startswith("SIM,"):
            self._handle_sim_line(line)
            return
        if upper.startswith("CAL,STATUS,"):
            self._handle_cal_status(line)
            return
        if upper.startswith("CAL,"):
            self._handle_cal_line(line)
            return

        data = self._parse_line(line)
        if data is None:
            return

        self.latest.update(data)
        self.yaw_data.append(data["yaw"])
        self.roll_data.append(data["roll"])
        self.pitch_data.append(data["pitch"])

    def _handle_cal_status(self, line: str):
        parts = line.split(",")
        imu = mag = False

        for item in parts[2:]:
            item = item.strip().upper()
            if item.startswith("IMU="):
                imu = item[4:] == "1"
            elif item.startswith("MAG="):
                mag = item[4:] == "1"

        if imu and mag:
            text = "校准参数：已加载"
        elif imu or mag:
            text = "校准参数：部分加载"
        else:
            text = "校准参数：未校准"

        self._show_hint(text)

    # ============================================================ 刷新 UI
    def refresh_ui(self):
        self._update_labels()
        self._update_health()
        self._update_curves()
        self._update_attitude()
        self._update_sim()

    def _update_labels(self):
        self.yaw_value.setText(f"{self.latest['yaw']:.1f}°")
        self.roll_value.setText(f"{self.latest['roll']:.1f}°")
        self.pitch_value.setText(f"{self.latest['pitch']:.1f}°")

    def _update_health(self):
        for key, widget in (("gyr", self.gyr_status), ("acc", self.acc_status),
                            ("mag", self.mag_status), ("bar", self.bar_status)):
            color = "green" if self.latest[key] else "red"
            widget.setStyleSheet(f"color: {color}; font-weight: bold;")

    def _update_curves(self):
        self.curve_yaw.setData(list(self.yaw_data))
        self.curve_roll.setData(list(self.roll_data))
        self.curve_pitch.setData(list(self.pitch_data))

    def _update_attitude(self):
        self.ahi.set_attitude(self.latest["roll"], self.latest["pitch"], self.latest["yaw"])

    # ============================================================ 退出
    def closeEvent(self, event):
        if self.serial_thread is not None:
            self.serial_thread.stop()
            self.serial_thread = None
        super().closeEvent(event)


# ---------------------------------------------------------------- 入口
def main():
    app = QApplication(sys.argv)
    
    font = app.font()
    font.setFamily("Segoe UI")
    font.setPointSize(11)
    app.setFont(font)

    window = MainWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()