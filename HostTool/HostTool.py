import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time
from collections import deque
import matplotlib
matplotlib.use('TkAgg')
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

root = tk.Tk()
root.title("Flight Control 上位机")
root.geometry("900x650")

N = 300
data_yaw   = deque([0]*N, maxlen=N)
data_roll  = deque([0]*N, maxlen=N)
data_pitch = deque([0]*N, maxlen=N)

ser = None
running = False

# ===== 串口控制栏 =====
top_frame = tk.Frame(root, pady=8)
top_frame.pack(fill=tk.X, padx=10)

tk.Label(top_frame, text="串口:", font=("Arial", 11)).pack(side=tk.LEFT)
port_var = tk.StringVar()
port_combo = ttk.Combobox(top_frame, textvariable=port_var, width=10, font=("Arial", 11))
port_combo.pack(side=tk.LEFT, padx=5)

def refresh_ports():
    ports = [p.device for p in serial.tools.list_ports.comports()]
    port_combo['values'] = ports
    if ports and not port_var.get():
        port_var.set(ports[0])

refresh_ports()
tk.Button(top_frame, text="刷新", command=refresh_ports).pack(side=tk.LEFT, padx=2)

status_label = tk.Label(top_frame, text="未连接", fg="red", font=("Arial", 11, "bold"))
status_label.pack(side=tk.LEFT, padx=20)

def toggle_connection():
    global ser, running
    if not running:
        try:
            port = port_var.get()
            if not port:
                messagebox.showerror("错误", "请选择串口")
                return
            ser = serial.Serial(port, 115200, timeout=0.1)
            running = True
            threading.Thread(target=serial_reader, daemon=True).start()
            status_label.config(text=f"已连接 {port}", fg="green")
            connect_btn.config(text="断开")
        except Exception as e:
            messagebox.showerror("连接失败", str(e))
    else:
        running = False
        time.sleep(0.2)
        if ser:
            ser.close()
            ser = None
        status_label.config(text="未连接", fg="red")
        connect_btn.config(text="连接")

connect_btn = tk.Button(top_frame, text="连接", command=toggle_connection, width=8)
connect_btn.pack(side=tk.LEFT, padx=10)

# ===== 数值显示区 =====
value_frame = tk.Frame(root, pady=10)
value_frame.pack(fill=tk.X, padx=20)

def make_value_box(parent, title, color):
    f = tk.Frame(parent, bg=color, relief=tk.RIDGE, bd=2, padx=15, pady=10)
    f.pack(side=tk.LEFT, expand=True, fill=tk.BOTH, padx=10)
    tk.Label(f, text=title, bg=color, fg="white", font=("Arial", 14, "bold")).pack()
    lbl = tk.Label(f, text="0.0°", bg=color, fg="white", font=("Consolas", 28, "bold"))
    lbl.pack()
    return lbl

lbl_yaw   = make_value_box(value_frame, "Yaw",   "#c0392b")
lbl_roll  = make_value_box(value_frame, "Roll",  "#27ae60")
lbl_pitch = make_value_box(value_frame, "Pitch", "#2980b9")

# ===== 曲线区 =====
fig = Figure(figsize=(9, 3.5), dpi=100)
ax = fig.add_subplot(111)
line_yaw,   = ax.plot(data_yaw,   label='Yaw',   color='#c0392b', linewidth=1.5)
line_roll,  = ax.plot(data_roll,  label='Roll',  color='#27ae60', linewidth=1.5)
line_pitch, = ax.plot(data_pitch, label='Pitch', color='#2980b9', linewidth=1.5)
ax.legend(loc='upper right')
ax.set_ylim(-180, 180)
ax.set_xlim(0, N)
ax.set_ylabel('Angle (deg)')
ax.set_title('Attitude')
ax.grid(True, alpha=0.3)
fig.tight_layout()

canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

# ===== 串口读取线程 =====
def serial_reader():
    while running:
        try:
            if ser and ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                parts = line.split(',')
                if len(parts) != 3:
                    continue
                yaw, roll, pitch = map(float, parts)
                data_yaw.append(yaw)
                data_roll.append(roll)
                data_pitch.append(pitch)
            else:
                time.sleep(0.005)
        except Exception:
            break

# ===== GUI 刷新 =====
def update_gui():
    lbl_yaw.config(text=f"{data_yaw[-1]:.1f}°")
    lbl_roll.config(text=f"{data_roll[-1]:.1f}°")
    lbl_pitch.config(text=f"{data_pitch[-1]:.1f}°")

    line_yaw.set_ydata(data_yaw)
    line_roll.set_ydata(data_roll)
    line_pitch.set_ydata(data_pitch)
    canvas.draw_idle()

    root.after(50, update_gui)

update_gui()
root.mainloop()

running = False
if ser:
    ser.close()