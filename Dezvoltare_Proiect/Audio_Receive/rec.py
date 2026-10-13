import serial
import wave
import struct

# configurare pentru portul serial
SERIAL_PORT = 'COM5'
BAUD_RATE = 115200
SAMPLE_RATE = 16000
OUTPUT_FILE = "t5838_record.wav"

print(f"Deschidere port serial {SERIAL_PORT}...")
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

pcm_data = bytearray()
RECORD_SECONDS = 5
total_bytes_to_read = SAMPLE_RATE * 2 * RECORD_SECONDS  # 16-bit = 2 octeti per esantion

print(f"Asteptare date audio de la STM32 ({RECORD_SECONDS} secunde)...")

try:
    while len(pcm_data) <total_bytes_to_read:
        raw_bytes = ser.read(1024)
        if raw_bytes:
            pcm_data.extend(raw_bytes)
            print(f"Progres: {len(pcm_data)} / {total_bytes_to_read} octeti", end='\r')

    print("\nInregistrare completa! Salvare fisier WAV...")

    with wave.open(OUTPUT_FILE, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm_data[:total_bytes_to_read])

    print(f"Fisier salvat cu succes: {OUTPUT_FILE}")

except KeyboardInterrupt:
    print("\nOprire manuala.")
finally:
    ser.close()