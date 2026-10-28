import sys
import os   
from collections import deque

import serial.tools.list_ports
import pyqtgraph as pg
from PyQt5.QtCore import Qt, QTimer, QSettings, QPointF, QEvent
from PyQt5.QtGui import QIcon, QPainter, QColor, QPen, QPolygonF
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QVBoxLayout, QMessageBox,
    QFrame, QLabel, QGridLayout, QWidget,
)
from PyQt5 import uic

from serial_thread import SerialThread
from theme import THEMES
from attitude_indicator import AttitudeIndicator

# ---------------------------------------------------------------- 常量
pg.setConfigOptions(antialias=True)

# 高 DPI（必须在 QApplication 之前设置）
QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

DATA_LEN       = 150          # 曲线缓存点数
REFRESH_MS     = 50           # 刷新周期
DEFAULT_BAUD   = 115200

# 协议字段位置（12 字段，逗号分隔）
IDX_YAW, IDX_ROLL, IDX_PITCH = 0, 1, 2
IDX_GYR, IDX_ACC, IDX_MAG, IDX_BAR = 8, 9, 10, 11
PROTOCOL_FIELDS = 12

# IMU 六面提示（物理朝向）
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

# 页面索引
PAGE_DASHBOARD   = 0
PAGE_CALIBRATION = 1
PAGE_CONFIG      = 2
PAGE_HELP        = 3
PAGE_ABOUT       = 4


def resource_path(relative_path: str) -> str:
    """获取资源绝对路径（兼容 PyInstaller 打包）"""
    if hasattr(sys, "_MEIPASS"):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), relative_path)


UI_PATH = resource_path("FC Configurator.ui")


# ---------------------------------------------------------------- 校准卡片
class FaceIcon(QWidget):
    """IMU 六面朝向的小板子示意图"""

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


# ---------------------------------------------------------------- 主窗口
class MainWindow(QMainWindow):

    def __init__(self):
        super().__init__()
        uic.loadUi(UI_PATH, self)
        self._set_window_icon()

        # --- 运行时状态 ---
        self.serial_thread = None
        self.latest = {
            "yaw": 0.0, "roll": 0.0, "pitch": 0.0,
            "gyr": 1, "acc": 1, "mag": 1, "bar": 1,
        }

        self.yaw_data   = deque([0] * DATA_LEN, maxlen=DATA_LEN)
        self.roll_data  = deque([0] * DATA_LEN, maxlen=DATA_LEN)
        self.pitch_data = deque([0] * DATA_LEN, maxlen=DATA_LEN)

        # --- UI 组装 ---
        self._build_plot()
        self._build_attitude_indicator()

        # --- 定时刷新 ---
        self.refresh_timer = QTimer(self)
        self.refresh_timer.setInterval(REFRESH_MS)
        self.refresh_timer.timeout.connect(self.refresh_ui)

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
            (self.btn_config,      PAGE_CONFIG),
            (self.btn_help,        PAGE_HELP),
            (self.btn_about,       PAGE_ABOUT),
        ]
        for btn, idx in self._nav_map:
            btn.clicked.connect(lambda _, i=idx: self._switch_page(i))

        self._switch_page(PAGE_DASHBOARD)

    def _switch_page(self, index: int):
        """切换页面 + 直接控制导航按钮颜色（不影响其他按钮）"""
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
        self.help_list.addItems([
            "飞控连接",
            "传感器校准",
            "实时数据",
            "常见问题"
        ])
        
        self._help_docs = {
            "飞控连接": """
                <h2>如何连接飞控？</h2>
                <p>1. 使用 USB 数据线将飞控连接到电脑。</p>
                <p>2. 在软件右上角的 <b>PORT</b> 下拉框中选择对应的串口(如 COM5)</p>
                <p>3. 点击 <b>CONNECT</b> 按钮.如果连接成功,状态栏会变为绿色并显示 CONNECTED</p>
                <p><i>提示：如果在下拉框中找不到串口，请点击旁边的刷新按钮 ↻ 重新扫描</i></p>
            """,
            "传感器校准": """
                <h2>传感器校准指南</h2>
                <p>左侧导航栏点击 <b>Calib</b> 进入校准页面</p>
                <ul>
                    <li><b>IMU</b> 先把 FACE1（箭头面朝上）放好点 Start；之后按图示逐面放好点 NEXT，最后陀螺阶段保持静止。</li>
                    <li><b>Mag</b> 远离金属物体，拿起飞行器缓慢翻转各个方向约 10 秒（不要只平着转 yaw）。</li>
                </ul>
                <p><i>气压计不需要校准，开机直接读取。</i></p>
                <p>校准过程中可以点 <b>STOP</b> 取消。</p>
            """,
            "实时数据": """
                <h2>实时数据监控</h2>
                <p>在主页面(Main),你可以实时查看飞行器的姿态信息</p>
                <p>左侧的 <b>姿态球</b> 会实时模拟飞行器的真实倾斜状态</p>
                <p>右侧的 <b>曲线图</b> 展示了 Yaw、Roll、Pitch 变化趋势,便于分析震动和姿态漂移</p>
            """,
            "常见问题": """
                <h2>常见问题 (FAQ)</h2>
                <p><b>Q: 点击 CONNECT 后提示连接失败？</b></p>
                <p>A: 请检查串口是否被其他软件占用,或者更换一根可通信的数据线</p>
                <br>
                <p><b>Q: 姿态球显示的数据一直在乱转？</b></p>
                <p>A: 可能是校准失败或罗盘受到干扰,请重新校准或远离磁场干扰</p>
            """
        }
        
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

        # 全局 QSS：只控制 Frame 背景和 Label 字体颜色
        self.setStyleSheet(f"""
            QLabel       {{ color: {t['label_color']}; }}
            QLabel#name  {{ color: {t['name_color']}; }}

            QFrame#top_frame       {{ background-color: {t['top_frame']}; }}
            QFrame#health_frame    {{ background-color: {t['health_frame']}; }}
            QFrame#plot_frame      {{ background-color: {t['plot_frame']}; }}
            QFrame#attitude_frame  {{ background-color: {t['attitude_frame']}; }}
            QFrame#nav_frame       {{ background-color: {t['nav_frame']}; }}
            QFrame#calib_frame     {{ background-color: {t['plot_frame']}; }}
            QFrame#about_frame     {{ background-color: {t['plot_frame']}; }}
            QFrame#line_frame      {{ background-color: {t['plot_grid']}; }}

            QFrame#faceTile {{
                border: 1px solid {t['plot_grid']};
                border-radius: 8px;
                background-color: rgba(255, 255, 255, 8);
            }}
            QFrame#faceTile[state="active"] {{ border: 2px solid {FACE_COLOR}; }}
            QFrame#faceTile[state="done"]   {{ border: 2px solid {FACE_COLOR}; }}

            QPlainTextEdit {{
                background-color: {t['plot_bg']};
                color: {t['label_color']};
                border: 1px solid {t['plot_grid']};
            }}

            QTextBrowser {{
                background-color: {t['plot_bg']};
                color: {t['label_color']};
                border: 1px solid {t['plot_grid']};
                padding: 10px;
            }}
            QListWidget {{
                background-color: {t['nav_frame']};
                color: {t['label_color']};
                border: 1px solid {t['plot_grid']};
                outline: none;
            }}
            QListWidget::item:selected {{
                background-color: {t['nav_checked_bg']};
                color: {t['nav_checked_fg']};
            }}
            
            QProgressBar {{
                background-color: {t['plot_grid']};
                border: 1px solid {t['plot_grid']};
                border-radius: 4px;
                text-align: center;
                color: {t['label_color']};
            }}
            QProgressBar::chunk {{
                background-color: {t['name_color']};
                border-radius: 3px;
            }}
        """)

        # ============================================================
        # 点名给普通按钮和下拉框加圆角，彻底告别系统直角
        # ============================================================
        normal_btns = [
            self.theme_button, self.refresh_button, self.link_button,
            self.start_accgyr, self.start_mag,
            self.reset_accgyr, self.reset_mag
        ]
        
        if theme_name == "dark":
            btn_bg, btn_border = "#1a2332", "#2a3a4a"
            btn_hover, btn_pressed = "#2a3a4a", "#0f1419"
        else:
            btn_bg, btn_border = "#ffffff", "#d0d7de"
            btn_hover, btn_pressed = "#f4f6f8", "#e1e4e8"

        btn_style = f"""
            QPushButton {{
                color: {t['label_color']};
                background-color: {btn_bg};
                border: 1px solid {btn_border};
                border-radius: 4px;
            }}
            QPushButton:hover {{ background-color: {btn_hover}; }}
            QPushButton:pressed {{ background-color: {btn_pressed}; }}
        """
        combo_style = f"""
            QComboBox {{
                color: {t['label_color']};
                background-color: {btn_bg};
                border: 1px solid {btn_border};
                border-radius: 4px;
                padding-left: 5px;
            }}
            QComboBox QAbstractItemView {{
                background-color: {btn_bg};
                color: {t['label_color']};
            }}
        """

        # 应用样式
        for btn in normal_btns:
            btn.setStyleSheet(btn_style)
        self.port_box.setStyleSheet(combo_style)

        # 曲线和姿态球
        self.plot.setBackground(t["plot_bg"])
        for ax_name in ("left", "bottom"):
            axis = self.plot.getAxis(ax_name)
            axis.setPen(t["plot_grid"])
            axis.setTextPen(t["plot_text"])

        self.ahi.set_colors(bg=t["plot_bg"], border=t["plot_grid"], text=t["plot_text"])
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