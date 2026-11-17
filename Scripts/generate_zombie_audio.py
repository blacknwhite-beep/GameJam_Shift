"""Synthesizes zombie melee attack snarl & eerie combat ambience (.wav)."""
import math
import os
import random
import struct
import wave

SR = 44100


def write_wav(path, left, right):
    peak = max(max(abs(v) for v in left), max(abs(v) for v in right), 1e-6)
    gain = 0.72 / peak
    frames = bytearray()
    for a, b in zip(left, right):
        frames += struct.pack("<hh", int(max(-1, min(1, a * gain)) * 32767), int(max(-1, min(1, b * gain)) * 32767))
    with wave.open(path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(frames)
    print(f"Generated {path} ({len(left) / SR:.2f}s)")


def gen_zombie_attack(path):
    dur = 0.42
    n = int(dur * SR)
    l, r = [0.0] * n, [0.0] * n
    random.seed(13)
    phase = 0.0
    lp = 0.0
    for i in range(n):
        t = i / SR
        norm = t / dur
        env = min(1.0, t / 0.025) * math.exp(-4.2 * norm)
        # Low guttural growl + FM rasp + slash noise
        freq = 135.0 * (1.0 - 0.35 * norm) + 28.0 * math.sin(2 * math.pi * 24.0 * t)
        phase += 2 * math.pi * freq / SR
        growl = math.tanh(2.2 * math.sin(phase) + 0.8 * math.sin(2.1 * phase))
        lp += 0.14 * (random.uniform(-1, 1) - lp)
        slash = lp * math.exp(-9.0 * norm) * 0.7
        s = (growl * 0.65 + slash) * env
        l[i] = s
        r[i] = s * 0.95
    write_wav(path, l, r)


def gen_zombie_ambience(path):
    dur = 10.0
    n = int(dur * SR)
    l, r = [0.0] * n, [0.0] * n
    random.seed(666)
    lp_l = lp_r = 0.0
    for i in range(n):
        t = i / SR
        # Deep eerie outbreak wind + ominous sub-drone
        gust = 0.5 + 0.35 * math.sin(2 * math.pi * 0.11 * t)
        lp_l += 0.012 * (random.uniform(-1, 1) - lp_l)
        lp_r += 0.012 * (random.uniform(-1, 1) - lp_r)
        drone = 0.18 * math.sin(2 * math.pi * 52.0 * t) + 0.10 * math.sin(2 * math.pi * 77.5 * t)
        l[i] = lp_l * 1.4 * gust + drone
        r[i] = lp_r * 1.4 * gust + drone * 0.95
    write_wav(path, l, r)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    gen_zombie_attack(os.path.join(here, "SFX_ZombieAttack.wav"))
    gen_zombie_ambience(os.path.join(here, "SFX_ZombieAmbience.wav"))
