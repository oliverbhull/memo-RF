# When SpeechEnd Is Triggered

SpeechEnd is emitted in exactly **one** place in the codebase.

## 1. VAD: `src/vad_endpointing.cpp` (lines 59–70)

**Condition:** All of the following must be true:

1. **State is Speech** – We previously got SpeechStart and are in `VADState::Speech`.
2. **Current frame is “not speech”** – `rms <= threshold_` (config `vad.threshold`, e.g. 0.09). So any frame with RMS at or below the speech threshold is treated as “silence” for end-of-utterance.
3. **Silence has lasted long enough** – `silence_samples_ >= end_silence_samples_`.  
   - `end_silence_samples_` is set from `vad.end_of_utterance_silence_ms` in config (no minimum; fully config-driven).

**Effect:** Any frame with RMS ≤ threshold (e.g. 0.09) is counted as silence. Natural speech dips (e.g. 0.05–0.08) therefore accumulate silence and can cause SpeechEnd after 2.5 s even if you are still speaking.

**No other code emits SpeechEnd.** The agent and state machine only *handle* the event once the VAD returns it; they do not create it.

## 2. How the agent uses SpeechEnd

- **`src/agent.cpp`**  
  - Around 317–330: if `vad_event == VADEvent::SpeechEnd`, the agent calls `handle_speech_end(...)`.  
  - `handle_speech_end` (394–417): updates time, finalizes the segment, enforces `min_speech_ms`, then runs STT and the rest of the pipeline.  
  There is no extra cutoff or timeout here; the only trigger is the VAD event above.

## 3. Fix for cutting off while still speaking

Use a **lower “silence” threshold**: only count a frame as silence (and add to `silence_samples_`) when RMS is below a low value (e.g. 0.02). Frames with RMS between that and the speech threshold (e.g. 0.02–0.09) do not add to silence, so normal speech dips no longer drive early SpeechEnd.
