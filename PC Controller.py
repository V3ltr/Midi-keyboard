
import json
import socket
import threading

import numpy as np
import sounddevice as sd

HOST = "127.0.0.1"
PORT = 8765

SAMPLE_RATE = 44100
BLOCK_SIZE = 512
BASE_NOTES = [60, 62, 64, 65, 67]

state_lock = threading.Lock()
active_notes = {}
volume = 0.5
bass = 0.5
pitch = 0.0
octave = 0


def midi_frequency(note):
    return 440.0 * (2.0 ** ((note - 69) / 12.0))


def audio_callback(outdata, frames, timing, status):
    with state_lock:
        notes = list(active_notes.values())
        current_volume = volume
        current_bass = bass
        current_pitch = pitch

    output = np.zeros(frames, dtype=np.float32)
    t = np.arange(frames, dtype=np.float64) / SAMPLE_RATE

    for frequency, velocity in notes:
        adjusted_frequency = frequency * (2.0 ** current_pitch)
        tone = np.sin(2 * np.pi * adjusted_frequency * t)
        tone += 0.35 * np.sin(4 * np.pi * adjusted_frequency * t)
        tone += 0.15 * np.sin(6 * np.pi * adjusted_frequency * t)
        output += tone.astype(np.float32) * velocity

    output *= 0.65 + current_bass * 0.7
    output *= current_volume * 0.15
    np.clip(output, -0.95, 0.95, out=output)

    outdata[:, 0] = output


def handle_event(message):
    global volume, bass, pitch, octave

    try:
        event = json.loads(message)
    except (ValueError, TypeError):
        print("Invalid message:", message)
        return

    with state_lock:
        event_type = event.get("type")

        if event_type == "touch":
            note_id = int(event.get("id", -1))

            if 0 <= note_id < len(BASE_NOTES):
                if event.get("pressed", False):
                    note = BASE_NOTES[note_id] + octave * 12
                    active_notes[note_id] = (midi_frequency(note), 0.7)
                else:
                    active_notes.pop(note_id, None)

        elif event_type == "controls":
            volume = max(0.0, min(1.0, float(event.get("volume", volume))))
            bass = max(0.0, min(1.0, float(event.get("bass", bass))))
            pitch = max(-1.0, min(1.0, float(event.get("pitch", pitch))))

        elif event_type == "octave":
            new_octave = max(-3, min(3, int(event.get("value", octave))))

            if new_octave != octave:
                octave = new_octave
                for note_id in list(active_notes):
                    note = BASE_NOTES[note_id] + octave * 12
                    active_notes[note_id] = (midi_frequency(note), 0.7)

    print("Received:", event)


def main():
    print("Starting piano audio...")

    with sd.OutputStream(
        samplerate=SAMPLE_RATE,
        blocksize=BLOCK_SIZE,
        channels=1,
        dtype="float32",
        callback=audio_callback
    ):
        print("Piano audio ready. Connecting to BLE bridge...")

        with socket.create_connection((HOST, PORT), timeout=15) as connection:
            connection.settimeout(None)
            print("Connected to BLE bridge.")

            buffer = b""

            while True:
                chunk = connection.recv(4096)

                if not chunk:
                    print("BLE bridge disconnected.")
                    break

                buffer += chunk

                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)

                    if line.strip():
                        handle_event(line.decode("utf-8", errors="replace"))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Piano stopped.")
    except (OSError, sd.PortAudioError) as error:
        print("Connection or audio error:", error)
