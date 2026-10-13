import serial
import wave
import time

COM_PORT = 'COM5'      # Ajustează portul COM corect
BAUD_RATE = 115200     # Sau 921600 dacă ai modificat în CubeMX
RECORD_TIME_SEC = 5    # Durată achiziție
SAMPLE_RATE = 16000    # Frecvența PCM rezultată din decimare (16 kHz)
OUTPUT_FILE = "t5838_real_audio.wav"

def record_audio():
    print(f"Deschidere port {COM_PORT}...")
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    
    print("Așteptare semnal WAKE de la STM32 (Bate din palme sau fă zgomot)...")
    
    pcm_data = bytearray()
    start_time = None
    
    while True:
        data = ser.read(ser.in_waiting or 1)
        if data:
            if start_time is None:
                print(">>> Detecție audio! Salvare flux PCM...")
                start_time = time.time()
            
            pcm_data.extend(data)
            
            # Progres
            elapsed = time.time() - start_time
            print(f"\rAchiziție: {elapsed:.1f}s / {RECORD_TIME_SEC}s ({len(pcm_data)} octeți)", end="")
            
            if elapsed >= RECORD_TIME_SEC:
                break

    ser.close()
    print("\n\nAchiziție finalizată! Generare fișier WAV...")

    # Salvare în format WAV standard
    with wave.open(OUTPUT_FILE, 'wb') as wf:
        wf.setnchannels(1)           # Mono
        wf.setsampwidth(2)          # 16-bit (2 octeți)
        wf.setframerate(SAMPLE_RATE) # 16 kHz
        wf.writeframes(pcm_data)

    print(f" Succes! Fișierul a fost salvat ca: {OUTPUT_FILE}")

if __name__ == "__main__":
    record_audio()