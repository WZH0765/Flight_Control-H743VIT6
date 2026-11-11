"""上位机公共常量：曲线缓存、协议字段、页面索引、仿真参数。"""

# ---------------- 刷新与缓存
DATA_LEN     = 150            # 曲线缓存点数
REFRESH_MS   = 50             # 刷新周期
DEFAULT_BAUD = 115200

# ---------------- 实机遥测协议（12 字段，逗号分隔）
IDX_YAW, IDX_ROLL, IDX_PITCH = 0, 1, 2
IDX_GYR, IDX_ACC, IDX_MAG, IDX_BAR = 8, 9, 10, 11
PROTOCOL_FIELDS = 12

# ---------------- 仿真遥测协议（15 字段）
# SIM,roll,pitch,yaw,gx,gy,gz,m1,m2,m3,m4,alt,arm,north,east   （姿态/角速度 ×10，位置 cm）
SIM_FIELDS   = 15
SIM_IDX_M1   = 7              # 四路油门在字段里的起始下标
SIM_IDX_N    = 13             # 北向位置（cm）
SIM_IDX_E    = 14             # 东向位置（cm）
SIM_MISS_MAX = 20             # 连续多少帧没数据判掉线（20×50ms≈1s）
SIM_PLOT_H   = 140            # 仿真页每张图的最小高度（四张图 + 地图挤在一列里）

# ---------------- 虚拟遥控
RC_SEND_MS  = 20              # 发包周期（ms）
RC_KEY_STEP = 8.0             # 键盘每周期推多少杆量

# ---------------- 页面索引
PAGE_DASHBOARD   = 0
PAGE_CALIBRATION = 1
PAGE_SIM         = 2
PAGE_HELP        = 3
PAGE_ABOUT       = 4

# ---------------- 仿真曲线配色
SIM_COLOR = {
    "roll":  "#2ed573", "pitch": "#a55eea", "yaw":   "#ff4757",
    "gx":    "#00a8ff", "gy":    "#e1b12c", "gz":    "#4cd137",
    "m1":    "#ff6b81", "m2":    "#ffa502", "m3":    "#1e90ff",
    "m4":    "#7bed9f", "alt":   "#e67e22",
}