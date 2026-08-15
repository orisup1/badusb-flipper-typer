#!/usr/bin/env python3
"""
Host-side helper for BadUSB Typing Race.

Connects to Flipper Zero via USB CDC (serial), receives the target word,
runs a local typing test in the terminal, calculates WPM, and sends the
result back to the Flipper.
"""
import sys
import time
import serial
import threading

def find_flipper_port():
    # Common names on Linux/macOS/Windows
    import glob
    candidates = glob.glob('/dev/ttyACM*') + glob.glob('/dev/tty.usbmodem*') + glob.glob('COM*')
    for p in candidates:
        try:
            s = serial.Serial(p, 115200, timeout=1)
            s.close()
            return p
        except Exception:
            pass
    return None

def read_line(ser):
    line = b''
    while True:
        ch = ser.read(1)
        if not ch:
            continue
        if ch in b'\r\n':
            if line:
                return line.decode(errors='ignore').strip()
        else:
            line += ch

def typing_test(word):
    print(f"\nType the word: {word}")
    print("Press Enter when done.")
    start = time.time()
    typed = sys.stdin.readline().strip()
    elapsed = time.time() - start
    if typed != word:
        print("❌  Mismatch! Try again.")
        return None
    # WPM = (chars/5) / minutes
    wpm = int((len(word) / 5.0) / (elapsed / 60.0))
    print(f"✅  {wpm} WPM ({elapsed:.2f}s)")
    return wpm

def main():
    port = find_flipper_port()
    if not port:
        print("Flipper Zero CDC port not found.")
        sys.exit(1)
    print(f"Using port {port}")

    ser = serial.Serial(port, 115200, timeout=0.1)
    time.sleep(0.5)          # allow CDC to settle

    while True:
        line = read_line(ser)
        if not line:
            continue
        print(f"[Flipper] {line}")
        if line.startswith("WPM:"):
            # Already a result, ignore
            continue
        # Assume line is the target word
        target = line.strip()
        wpm = typing_test(target)
        if wpm is not None:
            resp = f"WPM:{wpm}\n"
            ser.write(resp.encode())
            ser.flush()
            print(f"Sent to Flipper: {resp.strip()}")

if __name__ == "__main__":
    main()