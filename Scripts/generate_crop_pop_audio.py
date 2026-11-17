"""Synthesizes a crisp 'pop!' sound effect for crop harvesting (Scripts/SFX_CropPop.wav)."""
import math
import os
import struct
import wave


def generate_crop_pop_wav(path: str, duration: float = 0.16, sr: int = 44100) -> None:
    n = int(duration * sr)
    frames = bytearray()
    phase = 0.0
    for i in range(n):
        t = i / sr
        norm = t / duration
        # Classic upward pitch-pop (220 Hz -> 920 Hz in first 55ms, then crisp decay)
        freq = 220.0 + 700.0 * min(1.0, norm / 0.35) ** 0.7
        phase += 2.0 * math.pi * freq / sr
        env = min(1.0, t / 0.004) * math.exp(-16.0 * norm)
        sample = (math.sin(phase) * 0.8 + math.sin(2.0 * phase) * 0.2) * env
        val = max(-32767, min(32767, int(sample * 30000)))
        frames += struct.pack("<hh", val, val)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        wf.writeframes(frames)
    print(f"Generated {path} ({n} stereo frames)")


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    generate_crop_pop_wav(os.path.join(here, "SFX_CropPop.wav"))
