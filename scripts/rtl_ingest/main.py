#!/usr/bin/env python3
"""
RTL-SDR 7-channel ingest: wideband capture -> channelizer -> VAD -> Parakeet ASR -> POST feed.
Run separately from memo-RF; config in config/rtl_ingest.json.
"""

import argparse
import json
import sys
import threading
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CONFIG_PATH = REPO_ROOT / "config" / "rtl_ingest.json"


def load_config(path: Path) -> dict:
    with open(path, "r") as f:
        return json.load(f)


def run_ingest(config: dict, device_index: int = 0) -> None:
    from .channelizer import RTLChannelizerStream
    from .vad import SimpleVAD
    from .asr_parakeet import ParakeetASR
    from .feed_client import notify_feed
    import queue
    import concurrent.futures

    center_mhz = config["center_freq_mhz"]
    sample_rate = int(config.get("sample_rate", 2400000))
    freqs_mhz = config["frequencies_mhz"]
    if len(freqs_mhz) != 7:
        raise ValueError("config must have exactly 7 frequencies_mhz")
    center_hz = center_mhz * 1e6
    freqs_hz = [f * 1e6 for f in freqs_mhz]
    feed_url = config.get("feed_server_url", "http://localhost:5050/api/feed/notify")
    model_name = config.get("parakeet_model", "nvidia/parakeet-rnnt-0.6b")
    device = config.get("device", "cuda")
    vad_cfg = config.get("vad", {})
    vad_threshold = vad_cfg.get("threshold", 0.02)
    vad_min_speech_ms = vad_cfg.get("min_speech_ms", 400)
    vad_end_silence_ms = vad_cfg.get("end_silence_ms", 800)
    vad_frame_ms = vad_cfg.get("frame_duration_ms", 30)

    # ASR worker queue: (channel_1based, audio_float32)
    segment_queue = queue.Queue(maxsize=64)

    def asr_worker():
        asr = ParakeetASR(model_name=model_name, device=device)
        asr.load()
        while True:
            try:
                item = segment_queue.get(timeout=1.0)
                if item is None:
                    break
                ch, audio = item
                text = asr.transcribe(audio)
                if text:
                    ok = notify_feed(feed_url, text, channel=ch)
                    print(f"[CH{ch}] {text[:60]}{'...' if len(text) > 60 else ''} -> feed ok={ok}")
            except queue.Empty:
                continue
            except Exception as e:
                print(f"[ASR] error: {e}", file=sys.stderr)

    # Start ASR worker thread
    asr_thread = threading.Thread(target=asr_worker, daemon=False)
    asr_thread.start()

    # RTL + channelizer
    stream = RTLChannelizerStream(
        center_freq_hz=center_hz,
        sample_rate=sample_rate,
        frequencies_hz=freqs_hz,
        device_index=device_index,
    )
    vads = [
        SimpleVAD(
            threshold=vad_threshold,
            min_speech_ms=vad_min_speech_ms,
            end_silence_ms=vad_end_silence_ms,
            frame_duration_ms=vad_frame_ms,
        )
        for _ in range(7)
    ]

    print("Starting RTL-SDR capture and channelizer...")
    stream.start()
    try:
        while True:
            for ch in range(7):
                block = stream.get_audio_block(ch)
                if block is None:
                    continue
                seg = vads[ch].process(block)
                if seg is not None:
                    if segment_queue.full():
                        try:
                            segment_queue.get_nowait()
                        except queue.Empty:
                            pass
                    segment_queue.put((ch + 1, seg.copy()))
            time.sleep(0.001)
    except KeyboardInterrupt:
        print("Stopping...")
    finally:
        for ch in range(7):
            seg = vads[ch].flush()
            if seg is not None:
                segment_queue.put((ch + 1, seg))
        stream.stop()
        segment_queue.put(None)
        asr_thread.join(timeout=30.0)


def main():
    parser = argparse.ArgumentParser(description="RTL-SDR 7-channel ingest -> Parakeet ASR -> feed")
    parser.add_argument("--config", type=Path, default=CONFIG_PATH, help="Path to rtl_ingest.json")
    parser.add_argument("--device", type=int, default=0, help="RTL-SDR device index")
    args = parser.parse_args()
    if not args.config.is_file():
        print(f"Config not found: {args.config}", file=sys.stderr)
        sys.exit(1)
    config = load_config(args.config)
    run_ingest(config, device_index=args.device)


if __name__ == "__main__":
    main()
