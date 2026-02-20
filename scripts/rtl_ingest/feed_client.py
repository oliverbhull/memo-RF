#!/usr/bin/env python3
"""POST transcript events to the feed server with optional channel id."""

import json
import time
import urllib.request
from typing import Optional

def notify_feed(
    feed_url: str,
    transcript: str,
    channel: Optional[int] = None,
    session_id: Optional[str] = None,
) -> bool:
    """
    POST to feed_server_url (e.g. http://localhost:5050/api/feed/notify).
    Payload: event_type=transcript, data=transcript, channel=1..7 (optional), timestamp_ms, session_id.
    """
    if not transcript or not transcript.strip():
        return False
    payload = {
        "event_type": "transcript",
        "data": transcript.strip(),
        "timestamp_ms": int(time.time() * 1000),
        "session_id": session_id or f"rtl_ch{channel or 0}_{int(time.time())}",
        "persona_name": "RTL",
        "language": "en",
    }
    if channel is not None and 1 <= channel <= 7:
        payload["channel"] = channel
    body = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        feed_url,
        data=body,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status == 200
    except Exception:
        return False
