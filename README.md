STM32 Real-time Opus Compression & Dashboard
Dự án thực hiện thu âm từ máy tính, truyền dữ liệu qua USB-CDC để vi điều khiển STM32F411 (Black Pill) thực hiện nén dữ liệu bằng thuật toán Opus, sau đó truyền ngược lại PC để hiển thị thông số và lưu trữ.

Tính năng
- Nén thời gian thực (Real-time): Xử lý frame âm thanh 20ms với độ trễ cực thấp.
- Tỉ lệ nén ấn tượng: Đạt tỉ lệ nén lên đến 30x (từ 384kbps xuống ~12.8kbps) mà vẫn giữ được chất lượng âm thanh ổn định.
- Dashboard trực quan: Hiển thị CPU Load, RAM Usage, và chi tiết phân bổ Heap/Stack của Kit ngay trên màn hình máy tính.
- Hệ thống Framing tự chế: Sử dụng 2-byte length header giúp quản lý các gói tin Opus (VBR) mà không cần cấu trúc Ogg phức tạp trên Kit.

🛠️ Yêu cầu
1. Phần cứng (Hardware)
Kit STM32F411CEU6 (Black Pill).
Cáp USB Type-C (hỗ trợ truyền dữ liệu).
PC/Laptop chạy Windows (đã cài driver ST-Link và USB-CDC).

2. Phần mềm (Software)
STM32CubeIDE: Phiên bản 1.12.0 trở lên.
Python 3.10+.
Audacity: Để kiểm tra phổ tần và chất lượng âm thanh.

Hướng dẫn (Reproduce)

Bước 1: Chuẩn bị Firmware (STM32 Side)
- Mở STM32CubeIDE và Import project từ thư mục nen_project/.
- Kiểm tra cấu hình trong file .ioc:
- USB_DEVICE: Mode Communication Device Class (Virtual Port Com).
- Clock: 96MHz (hoặc tối đa của Kit).
- Build project (Ctrl + B).
- Kết nối Kit và Flash code xuống. Kit sẽ xuất hiện trong Device Manager dưới dạng STMicroelectronics Virtual COM Port (COMx).

Bước 2: Thiết lập môi trường Python (PC Side)
- Mở Terminal tại thư mục gốc.
- Kích hoạt môi trường ảo:

Bash
python -m venv .env
./.env/Scripts/acitvate
Cài đặt thư viện nếu chưa có:

Bash
pip install -r requirements.txt

Bước 3: Chạy Pipeline thu âm và Visualize
- Mở file full_pipeline.py.
- Sửa dòng PORT = 'COMx' cho khớp với cổng COM thực tế của Kit.

Chạy script:

Bash
python full_pipeline.py

- Quan sát Dashboard: Cột CPU sẽ nhảy lên khi bạn nói vào Micro (do Kit đang xử lý nén).

Bước 4: Giải mã và Kiểm tra kết quả
- Sau khi dừng script, bạn sẽ có file compressed.raw_opus.
- Sử dụng script giải mã để chuyển về định dạng WAV:

Bash
python opus_to_wav.py

- Mở decoded_output.wav bằng Audacity để nghe và so sánh với original.pcm.

Thông số kỹ thuật (Technical Specs)
- Audio Profile: 24,000 Hz, 16-bit, Mono.
- Opus Mode: VOIP (Low Latency).
- Frame Size: 20ms (480 samples).
- Complexity: 0-3 (Tối ưu cho vi điều khiển).
Memory Footprint:
- Heap: ~37 KB (Dành cho Opus State).
- Stack: ~22 KB.