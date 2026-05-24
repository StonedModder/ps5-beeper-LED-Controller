"""PS5 Beeper GUI — connects to beeper_server.elf running on PS5.

Deploy beeper_server.elf to the PS5 first (use the Deploy Payload button
or run manually):
    wsl bash -lc 'cat payload/beeper_server.elf | nc 192.168.69.44 9021'

Then run this script:
    python beeper_gui.py
"""

import os
import socket
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

DEFAULT_HOST = "192.168.69.44"
SERVER_PORT  = 9111   # beeper_server.elf listens here
DEPLOY_PORT  = 9021   # PS5 payload loader
TIMEOUT      = 3.0


def send_command(host: str, port: int, cmd: str) -> str:
    try:
        with socket.create_connection((host, port), timeout=TIMEOUT) as s:
            s.sendall((cmd + "\n").encode())
            data = b""
            while b"\n" not in data:
                chunk = s.recv(256)
                if not chunk:
                    break
                data += chunk
            return data.decode(errors="replace").strip()
    except OSError as e:
        return f"ERR connect: {e}"


def deploy_elf(host: str, path: str) -> str:
    try:
        with open(path, "rb") as f:
            data = f.read()
        with socket.create_connection((host, DEPLOY_PORT), timeout=10) as s:
            s.sendall(data)
        return f"OK deployed {os.path.basename(path)} ({len(data)} bytes)"
    except OSError as e:
        return f"ERR deploy: {e}"


class BeeperGUI(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PS5 Beeper Control")
        self.resizable(False, False)
        self._spamming = False
        self._build()

    def _build(self):
        pad = {"padx": 8, "pady": 5}

        # ── connection / deploy row ─────────────────────────────────────
        conn_frame = ttk.LabelFrame(self, text="Connection & Deploy")
        conn_frame.grid(row=0, column=0, columnspan=3, sticky="ew", **pad)

        ttk.Label(conn_frame, text="PS5 IP:").grid(row=0, column=0, sticky="w", padx=4)
        self.host_var = tk.StringVar(value=DEFAULT_HOST)
        ttk.Entry(conn_frame, textvariable=self.host_var, width=16).grid(row=0, column=1, padx=4)

        ttk.Button(conn_frame, text="Ping Beeper Server",
                   command=self._ping).grid(row=0, column=2, padx=8)

        ttk.Button(conn_frame, text="Deploy Payload (ELF)...",
                   command=self._deploy_payload).grid(row=0, column=3, padx=8)

        # ── beep type buttons ───────────────────────────────────────────
        beep_frame = ttk.LabelFrame(self, text="Beep Type")
        beep_frame.grid(row=1, column=0, sticky="nsew", **pad)

        beeps = [
            ("Silent (0)",        0, "#607D8B"),
            ("Single Beep (1)",   1, "#2196F3"),
            ("Error Pattern (2)", 2, "#F44336"),
            ("Long Beep (3)",     3, "#FF9800"),
        ]
        for i, (label, val, color) in enumerate(beeps):
            tk.Button(
                beep_frame, text=label,
                bg=color, fg="white",
                activebackground=color, activeforeground="white",
                font=("Segoe UI", 11, "bold"), width=20,
                command=lambda v=val: self._buzz(v),
            ).grid(row=i, column=0, pady=4, padx=8)

        # ── volume controls ─────────────────────────────────────────────
        vol_frame = ttk.LabelFrame(self, text="Beeper Volume")
        vol_frame.grid(row=1, column=1, sticky="nsew", **pad)

        self.vol_var = tk.IntVar(value=0)
        volumes = [("High (0)", 0), ("Medium (1)", 1), ("Low (2)", 2)]
        for i, (label, val) in enumerate(volumes):
            ttk.Radiobutton(
                vol_frame, text=label,
                variable=self.vol_var, value=val,
                command=lambda v=val: self._set_vol(v),
            ).grid(row=i, column=0, sticky="w", padx=8, pady=4)

        # ── LED brightness controls ─────────────────────────────────────
        led_frame = ttk.LabelFrame(self, text="LED Brightness")
        led_frame.grid(row=1, column=2, sticky="nsew", **pad)

        self.led_var = tk.IntVar(value=-1)
        # Confirmed 2026-05-24: 0=Bright 1=Medium 2=Dim
        leds = [("Bright (0)", 0), ("Medium (1)", 1), ("Dim (2)", 2)]
        for i, (label, val) in enumerate(leds):
            ttk.Radiobutton(
                led_frame, text=label,
                variable=self.led_var, value=val,
                command=lambda v=val: self._set_led(v),
            ).grid(row=i, column=0, sticky="w", padx=8, pady=4)

        # ── mute controls ───────────────────────────────────────────────
        mute_frame = ttk.LabelFrame(self, text="Mute")
        mute_frame.grid(row=2, column=0, sticky="ew", **pad)

        tk.Button(
            mute_frame, text="Mute ON",
            bg="#9C27B0", fg="white",
            activebackground="#9C27B0", activeforeground="white",
            font=("Segoe UI", 10, "bold"), width=12,
            command=lambda: self._set_mute(1),
        ).grid(row=0, column=0, padx=8, pady=4)

        tk.Button(
            mute_frame, text="Mute OFF",
            bg="#4CAF50", fg="white",
            activebackground="#4CAF50", activeforeground="white",
            font=("Segoe UI", 10, "bold"), width=12,
            command=lambda: self._set_mute(0),
        ).grid(row=0, column=1, padx=8, pady=4)

        # ── spam controls ───────────────────────────────────────────────
        spam_frame = ttk.LabelFrame(self, text="Spam Beep")
        spam_frame.grid(row=2, column=1, columnspan=2, sticky="ew", **pad)

        ttk.Label(spam_frame, text="Type (0-3):").grid(row=0, column=0, padx=4)
        self.spam_val = tk.IntVar(value=1)
        ttk.Spinbox(spam_frame, from_=0, to=3, textvariable=self.spam_val,
                    width=4).grid(row=0, column=1, padx=4)

        ttk.Label(spam_frame, text="Count:").grid(row=0, column=2, padx=4)
        self.spam_count = tk.IntVar(value=5)
        ttk.Spinbox(spam_frame, from_=1, to=100, textvariable=self.spam_count,
                    width=5).grid(row=0, column=3, padx=4)

        ttk.Label(spam_frame, text="Delay (ms):").grid(row=0, column=4, padx=4)
        self.spam_delay = tk.IntVar(value=400)
        ttk.Spinbox(spam_frame, from_=50, to=5000, increment=50,
                    textvariable=self.spam_delay, width=6).grid(row=0, column=5, padx=4)

        self.spam_btn = tk.Button(
            spam_frame, text="SPAM",
            bg="#E91E63", fg="white",
            activebackground="#E91E63", activeforeground="white",
            font=("Segoe UI", 10, "bold"), width=8,
            command=self._spam,
        )
        self.spam_btn.grid(row=0, column=6, padx=10)

        # ── status bar ──────────────────────────────────────────────────
        self.status_var = tk.StringVar(
            value="Ready — deploy beeper_server.elf to PS5 first, then Ping")
        ttk.Label(self, textvariable=self.status_var,
                  relief="sunken", anchor="w").grid(
            row=3, column=0, columnspan=3, sticky="ew", padx=8, pady=(0, 8))

    # ── helpers ─────────────────────────────────────────────────────────

    def _host(self):
        return self.host_var.get().strip()

    def _cmd(self, cmd: str) -> str:
        resp = send_command(self._host(), SERVER_PORT, cmd)
        self.status_var.set(f"{cmd}  →  {resp}")
        return resp

    def _buzz(self, val: int):
        self._cmd(f"BUZZ {val}")

    def _set_vol(self, val: int):
        self._cmd(f"VOL {val}")

    def _set_mute(self, val: int):
        self._cmd(f"MUTE {val}")

    def _set_led(self, val: int):
        self._cmd(f"LED {val}")

    def _ping(self):
        resp = self._cmd("BUZZ 1")
        if resp.startswith("OK"):
            messagebox.showinfo("Ping", f"Server reachable!\n{resp}")
        else:
            messagebox.showerror("Ping", f"No response:\n{resp}\n\nDeploy beeper_server.elf first.")

    def _deploy_payload(self):
        path = filedialog.askopenfilename(
            title="Select PS5 ELF payload",
            filetypes=[("PS5 ELF", "*.elf"), ("All files", "*.*")],
            initialdir=os.path.join(os.path.dirname(__file__), "payload"),
        )
        if not path:
            return
        self.status_var.set(f"Deploying {os.path.basename(path)}...")
        self.update()

        def _do():
            result = deploy_elf(self._host(), path)
            self.after(0, lambda: self.status_var.set(result))

        threading.Thread(target=_do, daemon=True).start()

    def _spam(self):
        if self._spamming:
            self._spamming = False
            self.spam_btn.config(text="SPAM")
            return
        self._spamming = True
        self.spam_btn.config(text="STOP")
        self._spam_remaining = int(self.spam_count.get())
        self._do_spam()

    def _do_spam(self):
        if not self._spamming or self._spam_remaining <= 0:
            self._spamming = False
            self.spam_btn.config(text="SPAM")
            return
        self._buzz(int(self.spam_val.get()))
        self._spam_remaining -= 1
        self.after(int(self.spam_delay.get()), self._do_spam)


if __name__ == "__main__":
    app = BeeperGUI()
    app.mainloop()
