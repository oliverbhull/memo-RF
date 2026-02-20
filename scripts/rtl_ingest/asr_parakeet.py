#!/usr/bin/env python3
"""
NVIDIA Parakeet ASR: NeMo when available, Hugging Face Transformers fallback on Jetson.
Accepts 16 kHz mono float audio, returns transcript text.
"""

import numpy as np
import tempfile
import os
from pathlib import Path
from typing import Optional, Any

AUDIO_RATE = 16000

# On Jetson we use HF Parakeet CTC (no NeMo). Use 1.1b: 0.6b repo lacks processor_config.json.
HF_FALLBACK_MODEL = "nvidia/parakeet-ctc-1.1b"


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


def _load_nemo(model_name: str, device: str) -> Any:
    """Load Parakeet via NeMo. Raises on failure (e.g. Jetson slim torch)."""
    import nemo.collections.asr as nemo_asr
    if "rnnt" in model_name.lower():
        model = nemo_asr.models.EncDecRNNTBPEModel.from_pretrained(model_name)
    else:
        model = nemo_asr.models.ASRModel.from_pretrained(model_name)
    if device == "cuda":
        model = model.cuda()
    else:
        model = model.cpu()
    return ("nemo", model)


def _ensure_hf_cache_writable() -> None:
    """Use a writable HF cache (avoid /mnt/nvme/cache or other read-only paths)."""
    home_cache = os.path.join(os.path.expanduser("~"), ".cache", "huggingface")
    for key in ("HF_HOME", "TRANSFORMERS_CACHE"):
        val = os.environ.get(key)
        if val and not (os.path.isdir(val) and os.access(val, os.W_OK)):
            os.environ[key] = home_cache
    os.environ.setdefault("HF_HOME", home_cache)
    os.environ.setdefault("TRANSFORMERS_CACHE", home_cache)


def _load_hf(model_name: str, device: str) -> Any:
    """Load Parakeet via Hugging Face Transformers (no NeMo, works on Jetson)."""
    from transformers import AutoModelForCTC, AutoProcessor
    _ensure_hf_cache_writable()
    processor = AutoProcessor.from_pretrained(HF_FALLBACK_MODEL)
    model = AutoModelForCTC.from_pretrained(HF_FALLBACK_MODEL)
    if device == "cuda":
        model = model.cuda()
    else:
        model = model.cpu()
    return ("hf", (processor, model))


class ParakeetASR:
    """
    Parakeet ASR: uses NeMo on full PyTorch, or Hugging Face Transformers on Jetson
    (where the PyTorch wheel has no torch.distributed).
    """

    def __init__(self, model_name: str = "nvidia/parakeet-rnnt-0.6b", device: str = "cuda"):
        self.model_name = model_name
        self.device = device
        self._backend: Optional[str] = None
        self._model: Any = None

    def load(self) -> None:
        # Prefer NeMo; on Jetson it often fails (slim torch has no torch._C._distributed_c10d).
        try:
            self._backend, self._model = _load_nemo(self.model_name, self.device)
            return
        except Exception:
            pass
        # Fallback: Hugging Face Parakeet CTC (no NeMo, no torch.distributed).
        try:
            self._backend, self._model = _load_hf(self.model_name, self.device)
            return
        except ImportError as e:
            raise ImportError(
                "NeMo failed (e.g. Jetson slim PyTorch). Fallback needs: pip install transformers"
            ) from e
        raise RuntimeError("NeMo and Hugging Face Parakeet fallback both failed.")

    def transcribe(self, audio: np.ndarray, sample_rate: int = AUDIO_RATE) -> str:
        if self._model is None:
            self.load()
        if sample_rate != AUDIO_RATE:
            from scipy import signal
            num = int(round(len(audio) * AUDIO_RATE / sample_rate))
            audio = signal.resample(audio, num).astype(np.float32)
        if len(audio) == 0:
            return ""

        if self._backend == "nemo":
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

        # Hugging Face
        processor, model = self._model
        inputs = processor(
            [audio],
            sampling_rate=AUDIO_RATE,
            return_tensors="pt",
            padding=True,
        )
        inputs = inputs.to(model.device)
        with np.errstate(divide="ignore", invalid="ignore"):
            outputs = model.generate(**inputs)
        text = processor.batch_decode(outputs, skip_special_tokens=True)
        return (text[0].strip() if text else "")
