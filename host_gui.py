#!/usr/bin/env python3
"""
Host-side GUI helper for BadUSB Typing Race (Tkinter).

Connects to Flipper Zero via USB CDC (serial), receives the target word,
shows a typing box, calculates WPM, and sends the result back to the Flipper.
"""
import sys
import time
import threading
import serial
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, messagebox

BAUD = 115200

class TypingRaceApp:
    def __init__(self, root):
        self.root = root
        self.root.title("BadUSB Typing Race – Host")
        self.ser = None
        self.reader_thread = None
        self.running = False

        self.build_ui()
        self.auto_connect()

    def build_ui(self):
        frm = ttk.Frame(self.root, padding=10)
        frm.grid()

        ttk.Label(frm, text="Port:").grid(row=0, column=0, sticky="w")
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(frm, textvariable=self.port_var, width=30, state="readonly")
        self.port_combo.grid(row=0, column=1, sticky="ew")
        self.refresh_ports()
        ttk.Button(frm, text="Refresh", command=self.refresh_ports).grid(row=0, column=2, padx=5)
        self.connect_btn = ttk.Button(frm, text="Connect", command=self.toggle_connect)
        self.connect_btn.grid(row=0, column=3, padx=5)

        ttk.Separator(frm, orient="horizontal").grid(row=1, column=0, columnspan=4, sticky="ew", pady=5)

        ttk.Label(frm, text="Target word:").grid(row=2, column=0, sticky="w")
        self.word_var = tk.StringVar()
        self.word_entry = ttk.Entry(frm, textvariable=self.word_var, width=40, state="readonly")
        self.word_entry.grid(row=2, column=1, columnspan=3, sticky="ew", pady=2)

        ttk.Label(frm, text="Type here:").grid(row=3, column=0, sticky="w")
        self.input_var = tk.StringVar()
        self.input_entry = ttk.Entry(frm, textvariable=self.input_var, width=40)
        self.input_entry.grid(row=3, column=1, columnspan=3, sticky="ew", pady=2)
        self.input_entry.bind("<Return>", self.on_submit)

        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(frm, textvariable=self.status_var, foreground="gray").grid(row=4, column=0, columnspan=4, pady=5)

        self.wpm_var = tk.StringVar()
        ttk.Label(frm, textvariable=self.wpm_var, font=("TkDefaultFont", 12, "bold")).grid(row=5, column=0, columnspan=4)

        frm.columnconfigure(1, weight=1)

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_combo["values"] = ports
        if ports:
            self.port_combo.current(0)

    def auto_connect(self):
        # try to find a likely Flipper port
        for p in serial.tools.list_ports.comports():
            if "ACM" in p.device or "usbmodem" in p.device or "USB" in p.description:
                self.port_var.set(p.device)
                self.toggle_connect()
                break

    def toggle_connect(self):
        if self.running:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        port = self.port_var.get()
        if not port:
            messagebox.showerror("Error", "No serial port selected")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to open {port}: {e}")
            return
        self.running = True
        self.connect_btn.config(text="Disconnect")
        self.status_var.set(f"Connected to {port}")
        self.reader_thread = threading.Thread(target=self.read_loop, daemon=True)
        self.reader_thread.start()

    def disconnect(self):
        self.running = False
        if self.ser:
            self.ser.close()
            self.ser = None
        self.connect_btn.config(text="Connect")
        self.status_var.set("Disconnected")
        self.word_var.set("")
        self.input_var.set("")
        self.wpm_var.set("")

    def read_loop(self):
        buffer = b""
        while self.running and self.ser:
            try:
                data = self.ser.read(64)
                if not data:
                    time.sleep(0.01)
                    continue
                buffer += data
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    line = line.strip().decode(errors="ignore")
                    self.root.after(0, self.handle_line, line)
            except Exception:
                break
        self.root.after(0, self.disconnect)

    def handle_line(self, line):
        if not line:
            return
        # Expect just the target word from Flipper
        self.word_var.set(line)
        self.input_var.set("")
        self.wpm_var.set("")
        self.input_entry.focus_set()

    def on_submit(self, event=None):
        typed = self.input_var.get().strip()
        target = self.word_var.get().strip()
        if not target:
            return
        if typed != target:
            messagebox.showwarning("Mismatch", "The typed word does not match. Try again.")
            return
        # Calculate WPM: (chars/5) / minutes
        # We don't have precise timing; approximate using time since word received.
        # For simplicity, use a fixed 30‑second window? Better: record start time when word received.
        # We'll store start time in attribute.
        if not hasattr(self, "word_start_time"):
            return
        elapsed = time.time() - self.word_start_time
        if elapsed <= 0:
            elapsed = 0.1
        wpm = int((len(target) / 5.0) / (elapsed / 60.0))
        self.wpm_var.set(f"WPM: {wpm}")
        # Send back to Flipper
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(f"WPM:{wpm}\n".encode())
                self.ser.flush()
            except Exception:
                pass
        # ready for next word
        self.word_start_time = None

    def set_word_start_time(self):
        self.word_start_time = time.time()

    # Override handle_line to capture start time
    def handle_line(self, line):
        if not line:
            return
        self.word_var.set(line)
        self.input_var.set("")
        self.wpm_var.set("")
        self.set_word_start_time()
        self.input_entry.focus_set()

def main():
    root = tk.Tk()
    app = TypingRaceApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.disconnect(), root.destroy()))
    root.mainloop()

if __name__ == "__main__":
    main()