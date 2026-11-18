from PyQt5.QtCore import QThread, pyqtSignal
import struct
import serial


def crc16(data: bytes) -> int:
    """和固件 Link.c 里的 CRC16 一致：多项式 0x1021，初值 0xFFFF"""
    crc = 0xFFFF

    for byte in data:
        crc ^= byte << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    return crc


class SerialThread(QThread):
    """读串口，把字节流拆成「二进制帧」和「文本行」两路"""

    line_received     = pyqtSignal(str)         # SIM,... / CAL,... / GPS,...
    frame_received    = pyqtSignal(int, list)   # (ID, int16 列表)
    connection_result = pyqtSignal(bool, str)   # (成功与否, 错误信息)

    def __init__(self, port, baud=115200):
        super().__init__()
        self.port = port
        self.baud = baud
        self.ser = None
        self.is_running = False
        self.buf = bytearray()

    def run(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.05)
            self.is_running = True
            self.connection_result.emit(True, "")

            while self.is_running:
                data = self.ser.read(max(1, self.ser.in_waiting))
                if data:
                    self._feed(data)
                else:
                    self.msleep(5)
        except Exception as e:
            self.connection_result.emit(False, str(e))

    def _feed(self, data):
        self.buf += data

        while self.buf:
            if len(self.buf) >= 2 and self.buf[0] == 0xAA and self.buf[1] == 0x55:
                if len(self.buf) < 4:
                    return

                length = self.buf[3]
                total = length + 6
                if len(self.buf) < total:
                    return

                body = bytes(self.buf[2:4 + length])
                crc = self.buf[4 + length] | (self.buf[5 + length] << 8)

                if length % 2 == 0 and crc16(body) == crc:
                    count = length // 2
                    values = list(struct.unpack("<%dh" % count, bytes(self.buf[4:4 + length])))
                    self.frame_received.emit(body[0], values)
                    del self.buf[:total]
                else:
                    del self.buf[0]         # 坏帧，丢一字节重新找同步

                continue

            # 文本行
            if self.buf[0] in (0x0A, 0x0D) or 0x20 <= self.buf[0] <= 0x7E:
                end = self.buf.find(b"\n")
                if end < 0:
                    if len(self.buf) > 4096:
                        del self.buf[:]
                    return

                line = bytes(self.buf[:end]).decode("utf-8", errors="ignore").strip()
                del self.buf[:end + 1]

                if line:
                    self.line_received.emit(line)

                continue

            del self.buf[0]                 # 二进制垃圾

    def stop(self):
        self.is_running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.wait()