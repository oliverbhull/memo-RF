# RTL-SDR 7-channel ingest

Separate pipeline from memo-RF: one RTL-SDR wideband capture, 7-channel software channelizer, VAD, NVIDIA Parakeet 0.6B ASR, POST to feed server with channel id.

## Setup

1. Install dependencies (from repo root or `scripts/rtl_ingest/`):
   ```bash
   pip install -r scripts/rtl_ingest/requirements.txt
   ```
2. Configure `config/rtl_ingest.json`: set the same 7 frequencies you program into your Baofeng UV-5R (memory 1–7). All 7 must fit within ~2.4 MHz for one RTL-SDR.
3. Start the feed server (so ingest can POST):
   ```bash
   python scripts/simple_feed.py
   ```
4. Run the ingest (with RTL-SDR plugged in):
   ```bash
   python scripts/run_rtl_ingest.py
   ```
   Options: `--config path/to/rtl_ingest.json`, `--device 0` (RTL-SDR device index).

## Config

- `center_freq_mhz`, `sample_rate`, `frequencies_mhz`: 7 frequencies in MHz (same order as UV-5R memory 1–7).
- `feed_server_url`: where to POST transcripts (default `http://localhost:5050/api/feed/notify`).
- `parakeet_model`: e.g. `nvidia/parakeet-rnnt-0.6b`.
- `device`: `cuda` or `cpu` for Parakeet.
- `vad`: optional `threshold`, `min_speech_ms`, `end_silence_ms`, `frame_duration_ms`.

## Flow

RTL-SDR → wideband IQ → channelizer (7× bandpass + NBFM + resample 16 kHz) → per-channel VAD → Parakeet transcribe → POST to feed with `channel` 1–7. The dashboard shows live transcripts with a CH badge when viewing the Demo feed.
