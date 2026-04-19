import struct
import wave
import os
import ctypes

# --- CẤU HÌNH ---
DLL_NAME = "libopus-0.dll" 
INPUT_FILE = "compressed.raw_opus"
OUTPUT_WAV = "decoded_output.wav"
SAMPLE_RATE = 24000
CHANNELS = 1
FRAME_SIZE = 480 # 20ms @ 24kHz

def main():
    # 1. Nạp thư viện Opus
    try:
        lib_path = os.path.join(os.getcwd(), DLL_NAME)
        lib = ctypes.CDLL(lib_path)
        print(f"--- [OK] Đã nạp {DLL_NAME} ---")
    except Exception as e:
        print(f"--- [Lỗi] Không tìm thấy DLL: {e} ---")
        return

    # Định nghĩa các hàm C
    lib.opus_decoder_create.restype = ctypes.c_void_p
    lib.opus_decode.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, 
                                ctypes.POINTER(ctypes.c_short), ctypes.c_int, ctypes.c_int]

    err = ctypes.c_int()
    decoder = lib.opus_decoder_create(SAMPLE_RATE, CHANNELS, ctypes.byref(err))
    
    if err.value != 0:
        print("Lỗi khởi tạo Decoder!")
        return

    # 2. Giải mã file
    if not os.path.exists(INPUT_FILE):
        print(f"Không tìm thấy file {INPUT_FILE}!")
        return

    with open(INPUT_FILE, "rb") as f_in, wave.open(OUTPUT_WAV, "wb") as f_out:
        f_out.setnchannels(CHANNELS)
        f_out.setsampwidth(2) # 16-bit
        f_out.setframerate(SAMPLE_RATE)
        
        pcm_buffer = (ctypes.c_short * (FRAME_SIZE * CHANNELS))()
        
        print("Đang giải mã...")
        while True:
            # Đọc 2 byte độ dài frame
            len_bytes = f_in.read(2)
            if not len_bytes: break
            
            length = struct.unpack('<H', len_bytes)[0]
            opus_packet = f_in.read(length)
            
            # Giải mã
            ret = lib.opus_decode(decoder, opus_packet, length, pcm_buffer, FRAME_SIZE, 0)
            if ret > 0:
                f_out.writeframes(pcm_buffer)
        
    print(f"--- Xong! Đã tạo file {OUTPUT_WAV} ---")

if __name__ == "__main__":
    main()