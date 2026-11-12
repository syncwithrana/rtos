#!/usr/bin/env python3
"""
LED Monitor for Embedded / RTOS Development (QEMU & Hardware)
Watches GPIO register writes via GDB and displays real-time RGB LED states in the terminal.
Guaranteed instant response to Ctrl+C.
"""

import argparse
import os
import queue
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time


def find_binary(name, default_candidates):
    if name:
        if shutil.which(name):
            return name
        sys.exit(f"Error: Specified binary '{name}' not found.")
    for candidate in default_candidates:
        path = shutil.which(candidate)
        if path:
            return path
    sys.exit(f"Error: None of {default_candidates} found in PATH.")


def make_led(is_on, bright_code, dim_code):
    if is_on:
        return f"\033[1;{bright_code}m( ● )\033[0m"
    return f"\033[2;{dim_code}m( ○ )\033[0m"


def kill_orphan_processes():
    """Kill any stale instances holding port 1234."""
    try:
        subprocess.run(
            ["killall", "-9", "qemu-system-arm", "arm-none-eabi-gdb"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=0.5,
        )
    except Exception:
        pass


def is_port_in_use(port, host="127.0.0.1"):
    try:
        with socket.create_connection((host, port), timeout=0.1):
            return True
    except (OSError, ConnectionRefusedError):
        return False


def wait_for_port(port, host="127.0.0.1", timeout=2.5):
    start = time.time()
    while time.time() - start < timeout:
        if is_port_in_use(port, host):
            return True
        time.sleep(0.04)
    return False


def main():
    parser = argparse.ArgumentParser(
        description="Monitor GPIO RGB LEDs in real-time via GDB watchpoints."
    )
    parser.add_argument(
        "--spawn-qemu", "--qemu", "-q",
        action="store_true",
        help="Automatically launch QEMU in the background and terminate it on exit",
    )
    parser.add_argument(
        "--elf", "-e",
        default="build/rtos.elf",
        help="Path to ELF binary (default: 'build/rtos.elf')",
    )
    parser.add_argument(
        "--target", "-t",
        default=":1234",
        help="GDB remote target connection string (default: ':1234')",
    )
    parser.add_argument(
        "--addr", "-a",
        default="0x400253FC",
        help="Memory address of GPIO DATA register (default: 0x400253FC / TM4C GPIOF)",
    )
    parser.add_argument(
        "--gdb",
        default=None,
        help="Explicit path to GDB executable",
    )
    parser.add_argument(
        "--qemu-bin",
        default=None,
        help="Explicit path to QEMU executable",
    )
    parser.add_argument(
        "--stream", "-s",
        action="store_true",
        help="Stream updates on new lines instead of in-place overwrite",
    )
    args = parser.parse_args()

    gdb_bin = find_binary(args.gdb, ["arm-none-eabi-gdb", "gdb-multiarch", "gdb"])
    elf_file = args.elf if os.path.isfile(args.elf) else None
    port = int(args.target.split(":")[-1]) if ":" in args.target else 1234

    qemu_proc = None
    gdb_proc = None
    script_path = None
    cleaned = False

    def cleanup(*_):
        nonlocal cleaned
        if cleaned:
            return
        cleaned = True

        # Restore terminal cursor & clear line
        try:
            sys.stdout.write("\033[?25h\r\033[K")
            sys.stdout.flush()
        except Exception:
            pass

        # Instant kill using SIGKILL
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

        os._exit(0)

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    # 1. Spawn QEMU if requested
    if args.spawn_qemu:
        if not elf_file:
            sys.exit(f"Error: ELF file '{args.elf}' not found. Run 'make' first.")

        # If port is already in use by an orphan QEMU from earlier, clear it
        if is_port_in_use(port):
            kill_orphan_processes()
            time.sleep(0.1)

        qemu_bin = find_binary(args.qemu_bin, ["qemu-system-arm"])
        qemu_cmd = [
            qemu_bin,
            "-M", "lm3s6965evb",
            "-nographic",
            "-s", "-S",
            "-kernel", elf_file,
        ]
        try:
            qemu_proc = subprocess.Popen(
                qemu_cmd,
                stdin=subprocess.DEVNULL,   # Never attach to TTY stdin so it cannot alter terminal modes
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                start_new_session=True,
            )
        except Exception as e:
            sys.exit(f"Failed to start QEMU ({qemu_bin}): {e}")

        # Wait for port to become available
        if not wait_for_port(port, timeout=2.0):
            if qemu_proc.poll() is not None:
                err = qemu_proc.stderr.read().decode(errors="replace")
                cleanup()
                sys.exit(f"QEMU failed to start:\n{err}")
            time.sleep(0.15)

    # 2. Build GDB watch script
    gdb_script = (
        "set pagination off\n"
        "set confirm off\n"
        f"target remote {args.target}\n"
        f"watch *(unsigned int *){args.addr}\n"
        "commands\n"
        "  silent\n"
        f'  printf "__LED_VAL__:0x%x\\n", *(unsigned int *){args.addr}\n'
        "  continue\n"
        "end\n"
        "continue\n"
    )

    with tempfile.NamedTemporaryFile("w", suffix=".gdb", delete=False) as tf:
        tf.write(gdb_script)
        script_path = tf.name

    cmd = [gdb_bin, "-q", "-nx", "-x", script_path]
    if elf_file:
        cmd.append(elf_file)

    try:
        gdb_proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,   # Never attach to TTY stdin
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            start_new_session=True,
        )
    except Exception as e:
        cleanup()
        sys.exit(f"Failed to launch GDB ({gdb_bin}): {e}")

    # Use a background reader thread so the main thread never blocks in C read()
    line_queue = queue.Queue()

    def reader_thread():
        try:
            for line in iter(gdb_proc.stdout.readline, ""):
                line_queue.put(line)
        except Exception:
            pass
        finally:
            line_queue.put(None)

    t = threading.Thread(target=reader_thread, daemon=True)
    t.start()

    # Hide terminal cursor for smooth in-place rendering
    if not args.stream:
        sys.stdout.write("\033[?25l")
        sys.stdout.flush()

    pattern = re.compile(r"__LED_VAL__:0x([0-9a-fA-F]+)")
    conn_fail_pattern = re.compile(
        r"(could not connect|Connection refused|Operation not permitted|Target is not responding)",
        re.IGNORECASE,
    )

    toggle_count = 0
    connected = False

    try:
        while True:
            try:
                line = line_queue.get(timeout=0.05)
            except queue.Empty:
                # Check if GDB died
                if gdb_proc.poll() is not None:
                    break
                continue

            if line is None:
                break

            if not connected and conn_fail_pattern.search(line):
                sys.stdout.write("\033[?25h\n")
                print(f"\033[1;31m[!] Connection failed: {line.strip()}\033[0m")
                print("    Tip: Run 'make kill' in case an orphan process holds port 1234.")
                break

            match = pattern.search(line)
            if match:
                connected = True
                val = int(match.group(1), 16)
                toggle_count += 1

                # PF1 = Red (0x02), PF2 = Blue (0x04), PF3 = Green (0x08)
                r_on = bool(val & (1 << 1))
                b_on = bool(val & (1 << 2))
                g_on = bool(val & (1 << 3))

                r_led = make_led(r_on, 91, 31)
                b_led = make_led(b_on, 94, 34)
                g_led = make_led(g_on, 92, 32)

                timestamp = time.strftime("%H:%M:%S")
                status = (
                    f"  {r_led}   {b_led}   {g_led}  "
                    f" \033[90m|\033[0m Reg: \033[1m0x{val:02X}\033[0m "
                    f"\033[90m|\033[0m Toggles: {toggle_count:<4} "
                    f"\033[90m|\033[0m {timestamp}"
                )

                if args.stream:
                    print(status)
                else:
                    sys.stdout.write(f"\r{status}\033[K")
                    sys.stdout.flush()

    except KeyboardInterrupt:
        pass
    finally:
        cleanup()


if __name__ == "__main__":
    main()
