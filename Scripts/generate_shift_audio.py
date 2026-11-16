import math
import os
import random
import struct
import wave


def generate_dimension_shift_wav(output_path: str, duration: float = 0.45, sample_rate: int = 44100) -> None:
    """Synthesizes a stereo dimensional whoosh + electric crackle stinger (.wav)."""
    num_samples = int(duration * sample_rate)
    random.seed(2026)

    frames = bytearray()
    phase_sub = 0.0
    phase_mid = 0.0

    for i in range(num_samples):
        t = i / sample_rate
        norm = t / duration

        # Fast attack (15ms), smooth exponential tail
        attack = min(1.0, t / 0.015)
        decay = math.exp(-5.2 * norm)
        env = attack * decay

        # 1) Deep sub-bass whoosh sweep: 190 Hz -> 42 Hz
        freq_sub = 190.0 * math.pow(42.0 / 190.0, norm)
        phase_sub += 2.0 * math.pi * freq_sub / sample_rate
        sub = math.sin(phase_sub) * 0.55

        # 2) Dimensional resonant sweep: 320 Hz -> 1450 Hz -> 500 Hz
        freq_mid = 320.0 + 1130.0 * math.sin(math.pi * norm)
        phase_mid += 2.0 * math.pi * freq_mid / sample_rate
        mid = math.sin(phase_mid) * 0.25 * math.exp(-3.5 * norm)

        # 3) Electric crackle / whoosh air noise with stereo shimmer
        noise_l = random.uniform(-1.0, 1.0)
        noise_r = random.uniform(-1.0, 1.0)
        crackle_gate = 1.0 if random.random() > 0.88 else 0.18
        crackle_env = math.exp(-7.5 * norm) * crackle_gate

        left = (sub + mid + noise_l * 0.35 * crackle_env) * env
        right = (sub + mid * 0.95 + noise_r * 0.35 * crackle_env) * env

        # Soft clip & convert to 16-bit signed PCM
        left_i16 = max(-32767, min(32767, int(math.tanh(left * 1.4) * 30000)))
        right_i16 = max(-32767, min(32767, int(math.tanh(right * 1.4) * 30000)))
        frames += struct.pack("<hh", left_i16, right_i16)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with wave.open(output_path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(frames)
    print(f"Generated {output_path} ({num_samples} stereo frames)")


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    generate_dimension_shift_wav(os.path.join(script_dir, "SFX_DimensionShift.wav"))
