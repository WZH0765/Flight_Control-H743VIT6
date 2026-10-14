import sys
import os
from collections import deque

import serial.tools.list_ports
import pyqtgraph as pg
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QMessageBox
from PyQt5.QtCore import Qt, QTimer, QSettings
from PyQt5 import uic

from PyQt5.QtGui import QIcon
from serial_thread import SerialThread
from theme import THEMES
from attitude_indicator import AttitudeIndicator

# 抗锯齿
pg.setConfigOptions(antialias=True)

# 高 DPI
QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)

def resource_path(relative_path):
    """获取资源绝对路径（兼容 PyInstaller 打包）"""
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), relative_path)

UI_PATH = resource_path("FC Configurator.ui")

DATA_LEN = 150

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        uic.loadUi(UI_PATH, self)
        # 窗口图标
        icon_path = resource_path("icon.ico")
        if os.path.exists(icon_path):
            self.setWindowIcon(QIcon(icon_path))
        self.serial_thread = None

        # 最新值缓存
        self.latest = {'yaw': 0.0, 'roll': 0.0, 'pitch': 0.0,
                       'gyr': 1, 'acc': 1, 'mag': 1, 'bar': 1}

        # 曲线数据
        self.yaw_data   = deque([0] * DATA_LEN, maxlen=DATA_LEN)
        self.roll_data  = deque([0] * DATA_LEN, maxlen=DATA_LEN)
        self.pitch_data = deque([0] * DATA_LEN, maxlen=DATA_LEN)

        # ---- pyqtgraph 曲线 ----
        self.plot = pg.PlotWidget()
        self.plot.setYRange(-180, 180)
        self.plot.showGrid(x=True, y=True, alpha=0.2)
        self.plot.addLegend(offset=(10, 10))
        self.curve_yaw   = self.plot.plot(pen=pg.mkPen('#ff4757', width=2), name='Yaw')
        self.curve_roll  = self.plot.plot(pen=pg.mkPen('#2ed573', width=2), name='Roll')
        self.curve_pitch = self.plot.plot(pen=pg.mkPen('#a55eea', width=2), name='Pitch')

        # ---- 姿态球 ----
        self.ahi = AttitudeIndicator()

        # ---- 塞进 Designer 里的 plot_frame / attitude_frame ----
        plot_layout = QVBoxLayout(self.plot_frame)
        plot_layout.setContentsMargins(0, 0, 0, 0)
        plot_layout.addWidget(self.plot)

        ahi_layout = QVBoxLayout(self.attitude_frame)
        ahi_layout.setContentsMargins(0, 0, 0, 0)
        ahi_layout.addWidget(self.ahi)

        # ---- 定时刷新 ----
        self.refresh_timer = QTimer()
        self.refresh_timer.timeout.connect(self.refresh_ui)

        # ---- 主题 ----
        self.settings = QSettings("FlightControl", "HostTool")
        self.current_theme = self.settings.value("theme", "light")
        self.theme_button.clicked.connect(self.toggle_theme)

        # ---- 串口 ----
        self.refresh_ports()
        self.refresh_button.clicked.connect(self.refresh_ports)
        self.link_button.clicked.connect(self.toggle_connection)

        # ---- 应用主题 ----
        self.apply_theme(self.current_theme)

    # ==================== 主题 ====================
    def apply_theme(self, theme_name):
        t = THEMES[theme_name]

        self.centralwidget.setStyleSheet(f"background-color: {t['centralwidget']};")
        self.top_frame.setStyleSheet(      f"background-color: {t['top_frame']};")
        self.health_frame.setStyleSheet(   f"background-color: {t['health_frame']};")
        self.value_frame.setStyleSheet(    f"background-color: {t['value_frame']};")
        self.plot_frame.setStyleSheet(     f"background-color: {t['plot_frame']};")
        self.attitude_frame.setStyleSheet( f"background-color: {t['attitude_frame']};")

        self.setStyleSheet(f"""
            QLabel       {{ color: {t['label_color']}; }}
            QLabel#name  {{ color: {t['name_color']}; }}
            QPushButton  {{ color: {t['button_fg']}; }}
            QComboBox    {{ color: {t['combobox_fg']}; }}
        """)

        self.plot.setBackground(t["plot_bg"])
        for ax_name in ['left', 'bottom']:
            axis = self.plot.getAxis(ax_name)
            axis.setPen(t["plot_grid"])
            axis.setTextPen(t["plot_text"])

        self.ahi.set_colors(
            bg=t["plot_bg"],
            border=t["plot_grid"],
            text=t["plot_text"],
        )

        self.theme_button.setText("☀" if theme_name == "light" else "🌙")

    def toggle_theme(self):
        self.current_theme = "dark" if self.current_theme == "light" else "light"
        self.apply_theme(self.current_theme)
        self.settings.setValue("theme", self.current_theme)

    # ==================== 弹窗 ====================
    def show_warning(self, title, text):
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

    # ==================== 串口 ====================
    def refresh_ports(self):
        self.port_box.clear()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            self.port_box.addItems(ports)

    def toggle_connection(self):
        if self.serial_thread is None:
            port = self.port_box.currentText()
            if not port:
                self.show_warning("提示", "请先选择串口")
                return
            self.serial_thread = SerialThread(port)
            self.serial_thread.line_received.connect(self.on_data)
            self.serial_thread.connection_result.connect(self.on_connection_result)
            self.serial_thread.start()
        else:
            self.serial_thread.stop()
            self.serial_thread = None
            self.refresh_timer.stop()
            self.link_button.setText("CONNECT")
            self.link_status.setText("NOT CONNECTED")

    def on_connection_result(self, success, error_msg):
        if success:
            self.link_button.setText("DISCONNECT")
            self.link_status.setText("CONNECTED")
            self.refresh_timer.start(50)
        else:
            self.serial_thread = None
            self.link_button.setText("CONNECT")
            self.link_status.setText("NOT CONNECTED")
            self.show_warning("连接失败",
                              f"无法打开 {self.port_box.currentText()}\n\n{error_msg}")

    # ==================== 数据 ====================
    def on_data(self, line):
        parts = line.split(',')
        if len(parts) != 12:
            return
        try:
            yaw   = float(parts[0])
            roll  = float(parts[1])
            pitch = float(parts[2])
            h_gyr = int(parts[8])
            h_acc = int(parts[9])
            h_mag = int(parts[10])
            h_bar = int(parts[11])
        except ValueError:
            return

        self.latest['yaw']   = yaw
        self.latest['roll']  = roll
        self.latest['pitch'] = pitch
        self.latest['gyr']   = h_gyr
        self.latest['acc']   = h_acc
        self.latest['mag']   = h_mag
        self.latest['bar']   = h_bar

        self.yaw_data.append(yaw)
        self.roll_data.append(roll)
        self.pitch_data.append(pitch)

    def refresh_ui(self):
        self.yaw_value.setText(f"{self.latest['yaw']:.1f}°")
        self.roll_value.setText(f"{self.latest['roll']:.1f}°")
        self.pitch_value.setText(f"{self.latest['pitch']:.1f}°")

        def color(ok):
            return "color: green; font-weight: bold;" if ok else "color: red; font-weight: bold;"
        self.gyr_status.setStyleSheet(color(self.latest['gyr']))
        self.acc_status.setStyleSheet(color(self.latest['acc']))
        self.mag_status.setStyleSheet(color(self.latest['mag']))
        self.bar_status.setStyleSheet(color(self.latest['bar']))

        self.curve_yaw.setData(list(self.yaw_data))
        self.curve_roll.setData(list(self.roll_data))
        self.curve_pitch.setData(list(self.pitch_data))

        # 姿态球
        self.ahi.set_attitude(self.latest['roll'],
                              self.latest['pitch'],
                              self.latest['yaw'])


app = QApplication(sys.argv)
font = app.font()
font.setFamily("Segoe UI")
font.setPointSize(11)
app.setFont(font)

window = MainWindow()
window.show()
sys.exit(app.exec_())