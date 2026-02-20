#!/usr/bin/env python3
"""
Wideband IQ capture + 7-channel bandpass, NBFM demodulation, resample to 16 kHz.
Consumes IQ from RTL-SDR (or test source) and yields 7 streams of 16 kHz mono float audio.
"""

import numpy as np
from scipy import signal
from typing import List, Iterator, Tuple, Optional
import threading
import queue
import time

# Defaults aligned with Baofeng UV-5R UHF and Option A (single RTL-SDR)
AUDIO_RATE = 16000  # Parakeet expects 16 kHz
NBFM_DEVIATION = 12500  # Hz, typical for 12.5 kHz NBFM
CHANNEL_BW = 12500  # Hz per channel (12.5 kHz spacing)


def _iq_bytes_to_complex(iq_bytes: np.ndarray) -> np.ndarray:
    """Convert RTL-SDR uint8 IQ (I,Q,I,Q,...) to complex float in [-1, 1]."""
    i = (iq_bytes[0::2].astype(np.float64) - 127.5) / 127.5
    q = (iq_bytes[1::2].astype(np.float64) - 127.5) / 127.5
    return i + 1j * q


def _nbfm_demod(complex_baseband: np.ndarray, sample_rate: float) -> np.ndarray:
    """
    NBFM demodulation: instantaneous frequency from phase derivative.
    complex_baseband: complex signal at sample_rate (after channel filter).
    Returns real audio in [-1, 1] at same sample_rate (caller will resample to 16k).
    """
    # Phase unwrap for stability
    phase = np.angle(complex_baseband)
    phase_unwrap = np.unwrap(phase)
    # Instantaneous frequency = d(phase)/(2*pi*dt)
    inst_freq = np.diff(phase_unwrap) * sample_rate / (2.0 * np.pi)
    # Pad to same length
    inst_freq = np.concatenate([[inst_freq[0]], inst_freq])
    # Scale to roughly [-1, 1] using typical deviation
    audio = inst_freq / NBFM_DEVIATION
    audio = np.clip(audio, -2.0, 2.0)
    return audio.astype(np.float32)


def _resample_to_16k(audio: np.ndarray, from_rate: float) -> np.ndarray:
    """Resample real float audio to 16 kHz."""
    if from_rate == AUDIO_RATE:
        return audio
    num = int(round(len(audio) * AUDIO_RATE / from_rate))
    return signal.resample(audio, num).astype(np.float32)


class Channelizer:
    """
    Takes wideband IQ at center_freq_hz and sample_rate.
    Extracts 7 channels (frequencies_hz), NBFM demodulates, resamples to 16 kHz.
    """

    def __init__(
        self,
        center_freq_hz: float,
        sample_rate: float,
        frequencies_hz: List[float],
        block_samples: int = 65536,
    ):
        if len(frequencies_hz) != 7:
            raise ValueError("frequencies_hz must have exactly 7 entries")
        self.center_freq_hz = center_freq_hz
        self.sample_rate = sample_rate
        self.frequencies_hz = list(frequencies_hz)
        self.block_samples = block_samples  # IQ samples per block (complex)
        # Offsets from center (Hz)
        self.offsets_hz = [f - center_freq_hz for f in self.frequencies_hz]
        # Design channel lowpass: cutoff ~CHANNEL_BW/2, order 5
        self.nyq = sample_rate / 2.0
        self.channel_cutoff = min(CHANNEL_BW / 2.0, self.nyq * 0.4)
        self.sos_channel = signal.butter(
            5, self.channel_cutoff / self.nyq, btype="low", output="sos"
        )
        # After demod we have audio at sample_rate; lowpass to 4 kHz then resample to 16k
        self.audio_cutoff = 4000.0  # speech bandwidth
        # Decimation: we'll lowpass then decimate to reduce rate before resample
        self.decim = max(1, int(sample_rate / 50000))  # target ~50 kHz for demod output
        self.demod_rate = sample_rate / self.decim
        self.sos_audio = signal.butter(
            5, self.audio_cutoff / (self.demod_rate / 2.0), btype="low", output="sos"
        )
        self._t = np.arange(block_samples, dtype=np.float64) / sample_rate

    def process_block(self, iq_complex: np.ndarray) -> List[np.ndarray]:
        """
        Process one block of complex IQ. Returns list of 7 float32 arrays at 16 kHz.
        """
        if len(iq_complex) < self.block_samples:
            # Pad with zeros
            pad = np.zeros(self.block_samples - len(iq_complex), dtype=iq_complex.dtype)
            iq_complex = np.concatenate([iq_complex, pad])
        elif len(iq_complex) > self.block_samples:
            iq_complex = iq_complex[: self.block_samples]

        out_audios = []
        for ch, offset_hz in enumerate(self.offsets_hz):
            # Mix to baseband
            mix = np.exp(-2j * np.pi * offset_hz * self._t[: len(iq_complex)])
            baseband = iq_complex * mix
            # Lowpass to channel bandwidth
            baseband = signal.sosfiltfilt(self.sos_channel, baseband)
            # Decimate to reduce rate for demod
            if self.decim > 1:
                baseband = baseband[:: self.decim]
                rate = self.demod_rate
            else:
                rate = self.sample_rate
            # NBFM demod
            audio = _nbfm_demod(baseband, rate)
            # Lowpass audio to 4 kHz
            audio = signal.sosfiltfilt(self.sos_audio, audio)
            # Resample to 16 kHz
            audio_16k = _resample_to_16k(audio, rate)
            out_audios.append(audio_16k)
        return out_audios


def _read_rtl_block(rtl, num_bytes: int) -> Optional[np.ndarray]:
    """Read one block of IQ bytes from pyrtlsdr RtlSdr. Returns complex or None on error."""
    try:
        raw = rtl.read_bytes(num_bytes)
        return _iq_bytes_to_complex(np.frombuffer(raw, dtype=np.uint8))
    except Exception:
        return None


class RTLChannelizerStream:
    """
    Runs RTL-SDR capture and channelizer in a thread; exposes 7 queues of 16 kHz audio chunks.
    """

    def __init__(
        self,
        center_freq_hz: float,
        sample_rate: float,
        frequencies_hz: List[float],
        block_samples: int = 65536,
        device_index: int = 0,
    ):
        self.center_freq_hz = center_freq_hz
        self.sample_rate = sample_rate
        self.frequencies_hz = frequencies_hz
        self.block_samples = block_samples
        self.device_index = device_index
        self.channelizer = Channelizer(
            center_freq_hz, sample_rate, frequencies_hz, block_samples
        )
        self.queues: List[queue.Queue] = [queue.Queue(maxsize=32) for _ in range(7)]
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._rtl = None

    def start(self) -> None:
        try:
            from rtlsdr import RtlSdr
        except ImportError:
            raise ImportError("pip install pyrtlsdr for RTL-SDR capture")

        self._rtl = RtlSdr(device_index=self.device_index)
        self._rtl.sample_rate = self.sample_rate
        self._rtl.center_freq = self.center_freq_hz
        self._rtl.gain = "auto"
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=5.0)
        if self._rtl:
            try:
                self._rtl.close()
            except Exception:
                pass
            self._rtl = None

    def _run(self) -> None:
        num_bytes = self.block_samples * 2  # 2 bytes per IQ sample (I and Q each uint8)
        while not self._stop.is_set():
            iq = _read_rtl_block(self._rtl, num_bytes)
            if iq is None:
                time.sleep(0.01)
                continue
            audios = self.channelizer.process_block(iq)
            for ch, audio in enumerate(audios):
                if self.queues[ch].full():
                    try:
                        self.queues[ch].get_nowait()
                    except queue.Empty:
                        pass
                self.queues[ch].put(audio)

    def get_audio_block(self, channel: int) -> Optional[np.ndarray]:
        """Get one block of 16 kHz audio for channel (0..6). Blocks until available or timeout."""
        try:
            return self.queues[channel].get(timeout=1.0)
        except queue.Empty:
            return None
