#!/usr/bin/env python3
import argparse
import os
import queue
import re
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

COLORS = {
    "G": (92, 32),
    "R": (91, 31),
    "B": (94, 34),
    "Y": (93, 33),
}


def parse_leds(arg):
    leds = []
    for item in arg.split(","):
        color, bit = item.strip().split(":")
        bright, dim = COLORS.get(color.upper(), (97, 90))
        leds.append((int(bit, 0), bright, dim))
    return leds


def make_led(is_on, bright, dim):
    return f"\033[1;{bright}m( ● )\033[0m" if is_on else f"\033[2;{dim}m( ○ )\033[0m"


def get_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_for_port(port, timeout=3.0):
    start = time.time()
    while time.time() - start < timeout:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--addr", required=True)
    parser.add_argument("--leds", required=True)
    parser.add_argument("--machine", default=None)
    parser.add_argument("--qemu", default="qemu-system-arm")
    parser.add_argument("--gdb", default="arm-none-eabi-gdb")
    parser.add_argument("--target", default=None)
    args = parser.parse_args()

    leds = parse_leds(args.leds)

    if args.machine:
        port = get_free_port()
        args.target = f":{port}"
    else:
        port = int(args.target.split(":")[-1]) if args.target and ":" in args.target else 1234
        if not args.target:
            args.target = ":1234"

    qemu_proc = None
    gdb_proc = None
    script_path = None

    def cleanup():
        sys.stdout.write("\033[?25h\r\033[K")
        sys.stdout.flush()
        if gdb_proc:
            try:
                gdb_proc.kill()
            except Exception:
                pass
        if qemu_proc:
            try:
                qemu_proc.kill()
            except Exception:
                pass
        if script_path and os.path.exists(script_path):
            try:
                os.remove(script_path)
            except Exception:
                pass

    def sig_exit(*_):
        cleanup()
        os._exit(0)

    signal.signal(signal.SIGINT, sig_exit)
    signal.signal(signal.SIGTERM, sig_exit)

    if args.machine:
        qemu_cmd = [args.qemu, "-M", args.machine, "-nographic", "-gdb", f"tcp::{port}", "-S", "-kernel", args.elf]
        try:
            qemu_proc = subprocess.Popen(
                qemu_cmd,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                start_new_session=True,
            )
        except Exception as e:
            sys.exit(f"Failed to start QEMU: {e}")

        if not wait_for_port(port):
            err = ""
            if qemu_proc and qemu_proc.poll() is not None:
                err = qemu_proc.stderr.read().decode(errors="replace")
            cleanup()
            sys.exit(f"Target port {port} not responding. {err}")

    gdb_script = (
        "set pagination off\n"
        "set confirm off\n"
        f"target remote {args.target}\n"
        f"watch *(unsigned int *){args.addr}\n"
        "commands\n"
        "  silent\n"
        f'  printf "__VAL__:0x%x\\n", *(unsigned int *){args.addr}\n'
        "  continue\n"
        "end\n"
        "continue\n"
    )

    with tempfile.NamedTemporaryFile("w", suffix=".gdb", delete=False) as tf:
        tf.write(gdb_script)
        script_path = tf.name

    try:
        gdb_proc = subprocess.Popen(
            [args.gdb, "-q", "-nx", "-x", script_path, args.elf],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            start_new_session=True,
        )
    except Exception as e:
        cleanup()
        sys.exit(f"Failed to start GDB: {e}")

    lines = queue.Queue()

    def reader():
        for line in iter(gdb_proc.stdout.readline, ""):
            lines.put(line)
        lines.put(None)

    threading.Thread(target=reader, daemon=True).start()

    sys.stdout.write("\033[?25l")
    sys.stdout.flush()

    pattern = re.compile(r"__VAL__:0x([0-9a-fA-F]+)")
    toggles = 0

    try:
        while True:
            try:
                line = lines.get(timeout=0.05)
            except queue.Empty:
                if gdb_proc.poll() is not None:
                    break
                continue

            if line is None:
                break

            match = pattern.search(line)
            if match:
                val = int(match.group(1), 16)
                toggles += 1
                indicators = [make_led(bool(val & (1 << bit)), bright, dim) for bit, bright, dim in leds]
                status = f"  {'   '.join(indicators)}   | Reg: 0x{val:02X} | Toggles: {toggles:<4} | {time.strftime('%H:%M:%S')}"
                sys.stdout.write(f"\r{status}\033[K")
                sys.stdout.flush()
    except KeyboardInterrupt:
        pass
    finally:
        cleanup()


if __name__ == "__main__":
    main()
