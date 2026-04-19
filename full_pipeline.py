import serial
import pyaudio
import threading
import queue
import matplotlib.pyplot as plt
import struct
from matplotlib.animation import FuncAnimation

# Cấu hình
PORT = 'COM6' # Cổng COM6
BAUD = 115200 # BAUD set cho có chứ không cần thiết do dùng USB-CDC
RATE = 24000  # Sampling rate       
FORMAT = pyaudio.paInt16 # Bit depth 16
CHUNK = 480 # 1 frame - 480 samples

audio_queue = queue.Queue(maxsize=100) # Hàng đợi frame âm thanh 
stats_queue = queue.Queue(maxsize=1) # Hàng đợi chứa list thông số để thống kê

def kit_worker(): # Luồng xử lí dữ liệu vào ra
    """Giao tiếp Kit: Nhận Log -> Thêm Header 2-byte -> Lưu Opus Binary"""
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1) # Khởi tạo kết nối serial
        ser.reset_input_buffer() # Reset buffer
        
        # Mở file ghi ở dạng binary 'wb'
        with open("original.pcm", "wb") as f_raw, open("compressed.raw_opus", "wb") as f_opus:
            print(f"--- [Connected] {PORT} - Đang lưu file với Length-Header ---")
            
            while True:
                if not audio_queue.empty(): # Nếu có frame trong hàng đợi thì chạy
                    pcm_frame = audio_queue.get() # Lấy ra frame 
                    f_raw.write(pcm_frame) # Ghi ngay vào file âm thanh gốc
                    ser.write(pcm_frame) # Gửi frame xuống kit

                    line = ser.readline().decode('utf-8', errors='ignore').strip() # Đợi kit nén xong và gửi dữ liệu lên
                    if line and line.endswith(',R'): # Kiểm tra đúng trạng thái ready nhận tiếp frame của kit
                        parts = line.split(',') # Chia dữ liệu thành một list bằng dấu ,
                        if len(parts) == 7: # Kiểm tra lần nữa xem đúng format dữ liệu chưa
                            try:
                                stats = [int(p) for p in parts[:6]] # Typecasting thông số từ string -> int
                                if stats_queue.full(): stats_queue.get() # Cập nhật thông số mới real-time
                                stats_queue.put(stats)

                                n_bytes = stats[5] # Đây là độ dài gói nén từ Kit gửi lên
                                if n_bytes > 0: # Kiểm tra size của gói tin nén gửi lên
                                    # Đọc đúng n_bytes dữ liệu Opus
                                    opus_data = ser.read(n_bytes)
                                    
                                    # Đóng gói n_bytes thành 2 byte (unsigned short, little-endian)
                                    header = struct.pack('<H', n_bytes) 
                                    
                                    # Ghi 2 byte độ dài trước, rồi mới ghi dữ liệu nén
                                    f_opus.write(header) # Ghi header trước
                                    f_opus.write(opus_data) # Ghi dữ liệu sau
                                    # ------------------------------------
                                    
                                    f_opus.flush() # Ép ghi luôn vào file
                            except ValueError: pass
    except Exception as e: 
        print(f"Serial Error: {e}")


        
def audio_thread(): # Luồng thu âm
    p = pyaudio.PyAudio() # Khởi tạo mic
    stream = p.open(format=FORMAT, channels=1, rate=RATE, input=True, frames_per_buffer=CHUNK) # Khởi tạo luồng stream
    while True:
        audio_queue.put(stream.read(CHUNK, exception_on_overflow=False)) # Đọc dữ liệu từ mic và đẩy vào queue

# DASHBOARD VISUALIZE 
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 6)) # Khởi tạo figure và axes

# Subplot 1: System Load
bars1 = ax1.bar(['CPU Load', 'RAM Load'], [0, 0], color=['#e74c3c', '#3498db'])
ax1.set_ylim(0, 110)
ax1.set_title('System Load Status', fontweight='bold', pad=15)
ax1.set_ylabel('Percentage (%)')

# Subplot 2: Memory Breakdown
bars2 = ax2.bar(['Static', 'Heap', 'Stack'], [0, 0, 0], color=['#2ecc71', '#f39c12', '#9b59b6'])
ax2.set_ylim(0, 140)
ax2.set_title('Memory Allocation Breakdown', fontweight='bold', pad=15)
ax2.set_ylabel('Usage (KB)')

def update_ui(frame):
    if not stats_queue.empty():
        s = stats_queue.get() # [cpu, ram, static, heap, stack, n_bytes]
        
        # 1. Xóa sạch text cũ
        for txt in list(ax1.texts): txt.remove()
        for txt in list(ax2.texts): txt.remove()

        # 2. Cập nhật bar height
        bars1[0].set_height(s[0]) # CPU load
        bars1[1].set_height(s[1]) # RAM load
        bars2[0].set_height(s[2]) # Static
        bars2[1].set_height(s[3]) # Heap
        bars2[2].set_height(s[4]) # Stack

        # 3. Viết chữ mới lên đầu cột
        # Load Status
        ax1.text(0, s[0] + 2, f"{float(s[0])}%", ha='center', weight='bold', fontsize=11)
        ax1.text(1, s[1] + 2, f"{float(s[1])}%", ha='center', weight='bold', fontsize=11)
        
        # Memory Breakdown
        ax2.text(0, s[2] + 2, f"{float(s[2])} KB", ha='center', weight='bold', fontsize=11)
        ax2.text(1, s[3] + 2, f"{float(s[3])} KB", ha='center', weight='bold', fontsize=11)
        ax2.text(2, s[4] + 2, f"{float(s[4])} KB", ha='center', weight='bold', fontsize=11)

    return bars1, bars2

# Khởi chạy các luồng
threading.Thread(target=audio_thread, daemon=True).start() # Luồng thu âm
threading.Thread(target=kit_worker, daemon=True).start() # Luồng dữ liệu vào ra

# Hàm FuncAnimation sẽ gọi update_ui mỗi 50ms để tạo hiệu ứng chuyển động
ani = FuncAnimation(fig, update_ui, interval=50, blit=False, cache_frame_data=False)
plt.tight_layout()
plt.show()