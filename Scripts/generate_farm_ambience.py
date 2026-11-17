"""Synthesizes a seamless looping farm ambience: gentle wind, rustling leaves and birdsong (stereo .wav)."""
import math
import os
import random
import struct
import wave

SR = 44100
DURATION = 12.0
FADE = 1.0  # seconds of loop crossfade


def synth(seed: int = 7):
    random.seed(seed)
    n = int((DURATION + FADE) * SR)
    left = [0.0] * n
    right = [0.0] * n

    # 1) Wind: low-passed noise with slow gust modulation, decorrelated per channel
    lp_l = lp_r = 0.0
    for i in range(n):
        t = i / SR
        gust = 0.55 + 0.30 * math.sin(2 * math.pi * 0.07 * t) + 0.15 * math.sin(2 * math.pi * 0.19 * t + 1.7)
        a = 0.015 + 0.01 * gust
        lp_l += a * (random.uniform(-1, 1) - lp_l)
        lp_r += a * (random.uniform(-1, 1) - lp_r)
        left[i] += lp_l * 1.6 * gust
        right[i] += lp_r * 1.6 * gust

    # 2) Leaf rustle: short high-passed noise bursts
    for _ in range(int(DURATION * 3)):
        start = random.randint(0, n - SR)
        length = int(random.uniform(0.15, 0.6) * SR)
        pan = random.random()
        amp = random.uniform(0.02, 0.05)
        prev = 0.0
        for k in range(length):
            env = math.sin(math.pi * k / length) ** 2
            x = random.uniform(-1, 1)
            hp = x - prev
            prev = x
            s = hp * env * amp
            left[start + k] += s * (1 - pan)
            right[start + k] += s * pan

    # 3) Birdsong: chirp phrases (frequency-swept sines with a 2nd harmonic)
    t_cursor = 0.3
    while t_cursor < DURATION:
        pan = random.uniform(0.15, 0.85)
        base = random.uniform(2600, 4200)
        notes = random.randint(2, 6)
        amp = random.uniform(0.05, 0.11)
        for _ in range(notes):
            dur = random.uniform(0.05, 0.14)
            sweep = random.uniform(-0.35, 0.45)
            start = int(t_cursor * SR)
            length = int(dur * SR)
            phase = 0.0
            for k in range(length):
                if start + k >= n:
                    break
                p = k / length
                f = base * (1 + sweep * p) * (1 + 0.03 * math.sin(2 * math.pi * 38 * k / SR))
                phase += 2 * math.pi * f / SR
                env = math.sin(math.pi * p) ** 1.5
                s = (math.sin(phase) + 0.25 * math.sin(2 * phase)) * env * amp
                left[start + k] += s * (1 - pan)
                right[start + k] += s * pan
            t_cursor += dur + random.uniform(0.03, 0.09)
        t_cursor += random.uniform(0.6, 2.2)

    # Seamless loop: crossfade the tail into the head
    out_n = int(DURATION * SR)
    fade_n = int(FADE * SR)
    out_l, out_r = left[:out_n], right[:out_n]
    for k in range(fade_n):
        w = k / fade_n
        out_l[k] = out_l[k] * w + left[out_n + k] * (1 - w)
        out_r[k] = out_r[k] * w + right[out_n + k] * (1 - w)
    return out_l, out_r


def write_wav(path, l, r):
    peak = max(max(abs(v) for v in l), max(abs(v) for v in r), 1e-6)
    gain = 0.7 / peak
    frames = bytearray()
    for a, b in zip(l, r):
        frames += struct.pack("<hh", int(max(-1, min(1, a * gain)) * 32767), int(max(-1, min(1, b * gain)) * 32767))
    with wave.open(path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(frames)
    print(f"Generated {path} ({len(l) / SR:.1f}s stereo loop)")


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    write_wav(os.path.join(here, "SFX_FarmAmbience.wav"), *synth())
