import serial
import subprocess
import time

ser = serial.Serial("/dev/serial0", 115200, timeout=1)  # Adjust port if needed
current_process = None
last_tx_command = None
last_heartbeat_time = time.time()


def send_to_esp32(message):
    """Send status messages to ESP32"""
    ser.write(f"{message}\n".encode("utf-8"))
    ser.flush()  # Ensure data is sent immediately


while True:
    try:
        command = ser.readline().decode("utf-8").strip()

        if command:
            print(f"Received: {command}")

            if command.startswith("TX"):
                _, freq, power = command.split()
                freq, power = int(freq), int(power)

                if current_process:
                    send_to_esp32("ERROR: Transmission already running")
                else:   
                    hackrf_cmd = f"hackrf_transfer -f {freq} -a 1 -x {power} -s 2000000"
                    current_process = subprocess.Popen(hackrf_cmd, shell=True)
                    last_tx_command = (freq, power)
                    send_to_esp32(f"TX_STARTED {freq}Hz {power}dBm")

            elif command == "STOP":
                if current_process:
                    subprocess.run("pkill hackrf_transfer", shell=True)
                    current_process = None
                    send_to_esp32("TX_STOPPED")
                else:
                    send_to_esp32("ERROR: No active transmission")

            elif command == "STATUS":
                if current_process:
                    send_to_esp32(
                        f"TX_ACTIVE {last_tx_command[0]}Hz {last_tx_command[1]}dBm"
                    )
                else:
                    send_to_esp32("TX_IDLE")

            elif command == "RESET":
                subprocess.run("pkill hackrf_transfer", shell=True)
                current_process = None
                send_to_esp32("TX_RESET")

        # Send heartbeat every 10 seconds
        if time.time() - last_heartbeat_time > 10:
            if current_process:
                send_to_esp32(
                    f"KEEP_ALIVE TX_ACTIVE {last_tx_command[0]}Hz {last_tx_command[1]}dBm"
                )
            else:
                send_to_esp32("KEEP_ALIVE TX_IDLE")
            last_heartbeat_time = time.time()

    except Exception as e:
        send_to_esp32(f"ERROR: {str(e)}")
