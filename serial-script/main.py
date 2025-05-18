# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "pyserial",
# ]
# ///
import os
import sys
import threading
import time
from pathlib import Path

import serial

if os.name == "nt":
    import msvcrt

    SERIAL_PORT = "COM7"  # Change to your port, e.g. /dev/ttyUSB0
else:
    import termios
    import tty

    SERIAL_PORT = "/dev/ttyUSB0"  # Change to your port, e.g. /dev/ttyUSB0

# Configuration
BAUD_RATE = 9600
RESPONSE_OUTPUT_FILE = Path("./output.txt")
# Global state
interactive_mode = False
running = True

servos_pos: list[int] = [0, 0, 0]


# Keypress reader (cross-platform)
def get_key():
    if os.name == "nt":
        return msvcrt.getch().decode("utf-8")

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(sys.stdin.fileno())
        ch = sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return ch


# Thread to read serial input
def serial_reader(ser):
    global servos_pos

    with RESPONSE_OUTPUT_FILE.open("w") as f:
        while running:
            if ser.in_waiting:
                line: str = ser.readline().decode(errors="ignore").strip()
                if line:
                    if line[0] == "$":
                        servos_pos = [int(v) for v in line[2:].split(" ")]
                        # print(servos_pos)
                        continue

                    f.write(f"[ATmega328P]: {line}\n")
                    f.flush()


# Interactive keypress sender
def interactive_input(ser):
    print(
        "** Interactive mode: Press keys to send. Type 'exit' to return to command mode. **"
    )

    ser.write("!interactive on".encode())
    try:
        while True:
            key = get_key()
            if key == "\x03":  # Ctrl+C
                break
            elif key == "\r" or key == "\n":
                continue
            elif key == "\x1b":  # ESC
                print("\n[Exiting interactive mode]")
                break
            elif key.lower() == "x":  # Optional: 'x' to exit
                print("\n[Exiting interactive mode]")
                break
            else:
                if key.lower() not in ["c", "d", "e", "f", "g", "a", "h"]:
                    print(f"Invalid note!: {key}")
                    continue
                ser.write(f"#{key}\n".encode())
                print(f"Sent: {key}")
    finally:
        ser.write("!interactive off\n".encode())


# Main control loop
def main():
    global interactive_mode, running

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        time.sleep(2)  # Wait for Arduino reset
    except serial.serialutil.SerialException as e:
        print(f"Cannot create serial connection: {e}")
        return

    try:
        threading.Thread(target=serial_reader, args=(ser,), daemon=True).start()

        while running:
            command = input(">>> ").strip()
            if command == "interactive":
                interactive_input(ser)
            elif command == "quit" or command == "exit":
                running = False
                break
            elif command:
                ser.write((command + "\n").encode())
    except KeyboardInterrupt:
        print("\n[Interrupted]")
    finally:
        ser.close()
        print("Serial closed.")


if __name__ == "__main__":
    main()
