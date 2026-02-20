#!/usr/bin/env python3
"""
Simple energy-based VAD on 16 kHz float audio. Buffers frames and emits segments on speech end.
"""

import numpy as np
from typing import List, Optional, Callable
import time

AUDIO_RATE = 16000


class SimpleVAD:
    """
    Frame-based VAD: speech when energy > threshold for min_speech_ms;
    segment ends after end_silence_ms of below-threshold.
    """

    def __init__(
        self,
        sample_rate: int = AUDIO_RATE,
        threshold: float = 0.02,
        min_speech_ms: int = 400,
        end_silence_ms: int = 800,
        frame_duration_ms: int = 30,
    ):
        self.sample_rate = sample_rate
        self.threshold = threshold
        self.min_speech_frames = max(1, min_speech_ms // frame_duration_ms)
        self.end_silence_frames = max(1, end_silence_ms // frame_duration_ms)
        self.frame_samples = sample_rate * frame_duration_ms // 1000
        self._speech_frames = 0
        self._silence_frames = 0
        self._buffer: List[np.ndarray] = []
        self._in_speech = False

    def process(self, audio: np.ndarray) -> Optional[np.ndarray]:
        """
        Process a chunk of audio (any length). Returns one complete segment (float32)
        when speech ends, or None.
        """
        offset = 0
        while offset < len(audio):
            frame = audio[offset : offset + self.frame_samples]
            offset += len(frame)
            if len(frame) < self.frame_samples:
                break
            rms = np.sqrt(np.mean(frame.astype(np.float64) ** 2))
            if rms >= self.threshold:
                self._silence_frames = 0
                self._speech_frames += 1
                if not self._in_speech:
                    self._buffer = []
                    self._in_speech = True
                self._buffer.append(frame.copy())
            else:
                if self._in_speech:
                    self._silence_frames += 1
                    self._buffer.append(frame.copy())
                    if self._silence_frames >= self.end_silence_frames:
                        if self._speech_frames >= self.min_speech_frames:
                            segment = np.concatenate(self._buffer)
                            self._buffer = []
                            self._in_speech = False
                            self._speech_frames = 0
                            self._silence_frames = 0
                            return segment
                        self._in_speech = False
                        self._buffer = []
                        self._speech_frames = 0
                        self._silence_frames = 0
                else:
                    self._silence_frames = 0
        return None

    def flush(self) -> Optional[np.ndarray]:
        """Emit any buffered speech as one segment."""
        if self._in_speech and len(self._buffer) > 0 and self._speech_frames >= self.min_speech_frames:
            segment = np.concatenate(self._buffer)
            self._buffer = []
            self._in_speech = False
            self._speech_frames = 0
            self._silence_frames = 0
            return segment
        return None
