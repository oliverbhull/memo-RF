# Wake Word ("Hey memo") and Half-Duplex Channel Clear

## Overview

When **wake word** is enabled (default), the agent:

1. **Transcribes everything** on the channel (continual STT). Every VAD segment is sent through speech-to-text; nothing is gated from STT.
2. **Responds only when** the transcript contains "hey memo" (case-insensitive). The phrase is stripped and the remainder is used as the command. All other segments are transcribed but not answered—the agent stays silent on the channel.
3. **Waits for channel clear** before keying up (half-duplex). After building a response, the agent does not transmit immediately. It waits until there has been no speech for `tx.channel_clear_silence_ms` (default 500 ms). If someone else keys up and talks while we are waiting, we do not transmit; we wait until they are done and then wait again for that silence period before transmitting.

## Configuration

In `config/config.json`:

```json
{
  "wake_word": {
    "enabled": true
  },
  "tx": {
    "channel_clear_silence_ms": 500
  }
}
```

- **`wake_word.enabled`** (default: `true`): When `true`, respond only when the transcript contains "hey memo". When `false`, legacy behavior: respond to every utterance (no wake phrase; transmit as soon as response is ready).
- **`tx.channel_clear_silence_ms`** (default: `500`): Half-duplex channel clear. After we have a response ready, we wait until we have observed **no speech** for this many milliseconds before keying up and transmitting. If we detect speech (someone else talking) while waiting, we wait until they finish (SpeechEnd) and then wait again for `channel_clear_silence_ms` of silence.

## Flow

1. **IdleListening** → any **SpeechStart** → **ReceivingSpeech** (we always receive and segment).
2. **ReceivingSpeech** → **SpeechEnd** → we run STT on the segment (continual STT).
   - If the transcript does **not** contain "hey memo" → we do nothing (no response); state goes to **IdleListening**.
   - If the transcript **does** contain "hey memo" → we strip "hey memo", use the rest as the command, run router/LLM/TTS to build the response, then enter **WaitingForChannelClear** (we do **not** key up yet).
3. **WaitingForChannelClear** → each frame we check:
   - If **SpeechStart** (someone keys up) → we go to **ReceivingSpeech** (we are hearing the interrupt; we do not transmit).
   - If we are in **ReceivingSpeech** (interrupt) and get **SpeechEnd** (they unkey) → we go back to **WaitingForChannelClear** and reset the channel-clear timer.
   - If we have been in **WaitingForChannelClear** and **(now − last_speech_end_time) ≥ channel_clear_silence_ms** → channel is clear → we transition to **Transmitting** and call `tx_->transmit(pending_response_audio)`.
4. **Transmitting** → when playback is complete → **IdleListening**.

## Why Half-Duplex Channel Clear?

The radio is half-duplex: only one party can transmit at a time. If the user says "Hey memo, what's the weather?" and unkeys, but then someone else keys up and starts talking before we key up, we must **not** key up and talk over them. So we wait. When they are done (SpeechEnd) and we have observed silence for `channel_clear_silence_ms`, we then key up and send our response.

## Disabling Wake Word (Legacy Mode)

Set `wake_word.enabled` to `false` in `config.json`. The agent will then respond to every utterance (no "hey memo" check) and will transmit as soon as the response is ready (no waiting for channel clear). This matches the original behavior before wake-word and channel-clear were added.
