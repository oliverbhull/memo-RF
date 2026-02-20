#!/usr/bin/env python3
"""
NVIDIA Parakeet 0.6B ASR via NeMo. Accepts 16 kHz mono float audio, returns transcript text.
"""

import numpy as np
import tempfile
import os
from pathlib import Path
from typing import Optional

AUDIO_RATE = 16000


def _patch_torch_distributed_for_nemo() -> None:
    """
    Jetson PyTorch wheels (and some minimal builds) do not expose GradBucket in
    torch.distributed, which NeMo imports for DDP hooks. We only need ASR inference,
    so stub GradBucket so the import chain succeeds.
    """
    import torch.distributed as dist
    if getattr(dist, "GradBucket", None) is not None:
        return
    try:
        from torch._C._distributed_c10d import GradBucket as _GradBucket
        dist.GradBucket = _GradBucket
        return
    except Exception:
        pass
    # Minimal stub so NeMo's debugging_hooks import succeeds; unused at inference time.
    class _GradBucketStub:
        def __init__(self, *args, **kwargs): pass
        def index(self): return 0
        def buffer(self): return None
    dist.GradBucket = _GradBucketStub


def _float_to_wav_bytes(audio: np.ndarray, rate: int = AUDIO_RATE) -> bytes:
    """Convert float32 [-1,1] to 16-bit PCM WAV bytes."""
    import wave
    import io
    audio_i16 = (np.clip(audio, -1.0, 1.0) * 32767).astype(np.int16)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(audio_i16.tobytes())
    return buf.getvalue()


class ParakeetASR:
    """
    Loads NeMo Parakeet (e.g. parakeet-rnnt-0.6b), transcribes 16 kHz mono audio.
    """

    def __init__(self, model_name: str = "nvidia/parakeet-rnnt-0.6b", device: str = "cuda"):
        self.model_name = model_name
        self.device = device
        self._model = None

    def load(self) -> None:
        _patch_torch_distributed_for_nemo()
        try:
            import nemo.collections.asr as nemo_asr
        except ImportError:
            raise ImportError(
                "NeMo ASR required: pip install nemo_toolkit[asr] (or nemo_toolkit['all'])"
            )
        # RNNT model uses EncDecRNNTBPEModel
        if "rnnt" in self.model_name.lower():
            self._model = nemo_asr.models.EncDecRNNTBPEModel.from_pretrained(
                self.model_name
            )
        else:
            # CTC or TDT: use generic ASRModel
            self._model = nemo_asr.models.ASRModel.from_pretrained(self.model_name)
        if self.device == "cuda":
            self._model = self._model.cuda()
        else:
            self._model = self._model.cpu()

    def transcribe(self, audio: np.ndarray, sample_rate: int = AUDIO_RATE) -> str:
        """
        Transcribe one segment. audio: float32 mono at 16 kHz (or resampled internally if not).
        Returns transcript text (strip/empty if nothing).
        """
        if self._model is None:
            self.load()
        if sample_rate != AUDIO_RATE:
            from scipy import signal
            num = int(round(len(audio) * AUDIO_RATE / sample_rate))
            audio = signal.resample(audio, num).astype(np.float32)
        if len(audio) == 0:
            return ""
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as f:
            f.write(_float_to_wav_bytes(audio, AUDIO_RATE))
            path = f.name
        try:
            hypotheses = self._model.transcribe([path])
            if hypotheses and len(hypotheses) > 0:
                text = getattr(hypotheses[0], "text", None) or str(hypotheses[0])
                return (text or "").strip()
            return ""
        finally:
            try:
                os.unlink(path)
            except Exception:
                pass
