import serial, wave, time
import numpy as np

COM_PORT    = 'COM5'
BAUD_RATE   = 921600
SAMPLE_RATE = 18750          # 2.4 MHz / 128
OUTPUT_FILE = "t5838_audio.wav"
RAW_FILE    = "t5838_raw.bin"
START_MARK  = b"[WAKE] Pornire captura PDM...\r\n"
END_MARK    = b"\r\n[SLEEP]"
TIMEOUT_S   = 60

def record_audio():
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.2)
    print(f"{COM_PORT} deschis. Apasa RESET pe placa (sau asteapta WAKE)...")

    buf = bytearray()
    start_idx = None
    t0 = time.time()

    while time.time() - t0 < TIMEOUT_S:
        chunk = ser.read(ser.in_waiting or 1)
        if not chunk:
            continue
        buf.extend(chunk)

        if start_idx is None:
            i = buf.find(START_MARK)
            if i >= 0:
                start_idx = i + len(START_MARK)
                print(">>> Captura pornita, primesc PCM...")
        else:
            print(f"\r{len(buf) - start_idx} octeti", end="")
            if buf.find(END_MARK, start_idx) >= 0:
                break
    ser.close()

    if start_idx is None:
        print("\nNu am vazut '[WAKE]'. Text primit:")
        print(buf.decode(errors="replace"))
        return

    end_idx = buf.find(END_MARK, start_idx)
    if end_idx < 0:
        end_idx = len(buf)
        print("\nAtentie: nu am primit '[SLEEP]', datele pot fi incomplete.")

    print("\n--- Log boot ---")
    print(buf[:start_idx].decode(errors="replace"))

    pcm = bytes(buf[start_idx:end_idx])
    pcm = pcm[: len(pcm) // 2 * 2]                 # numar par de octeti
    open(RAW_FILE, "wb").write(pcm)

    x = np.frombuffer(pcm, dtype="<i2")
    print(f"{len(x)} esantioane = {len(x)/SAMPLE_RATE:.2f} s "
          f"(asteptat ~5 s - ~10 ms)")
    if len(x):
        print(f"peak={np.abs(x).max()}  rms={np.sqrt(np.mean(x.astype(float)**2)):.1f}")

    with wave.open(OUTPUT_FILE, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm)
    print(f"Salvat: {OUTPUT_FILE}")

if __name__ == "__main__":
    record_audio()