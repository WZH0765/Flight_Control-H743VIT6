from PyQt5.QtCore import QThread, pyqtSignal
import serial


class SerialThread(QThread):
    line_received     = pyqtSignal(str)
    connection_result = pyqtSignal(bool, str)   # (成功与否, 错误信息)

    def __init__(self, port, baud=115200):
        super().__init__()
        self.port = port
        self.baud = baud
        self.ser = None
        self.is_running = False

    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1)
            self.is_running = True
            self.connection_result.emit(True, "")    # ← 通知主线程成功

            while self.is_running:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.line_received.emit(line)
                else:
                    self.msleep(5)
        except Exception as e:
            self.connection_result.emit(False, str(e))   # ← 通知主线程失败

    def stop(self):
        self.is_running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.wait()