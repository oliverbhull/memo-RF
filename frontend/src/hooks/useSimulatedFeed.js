import { useState, useEffect } from 'react';

/**
 * Fetch GET /api/simulated/feed and group by channel.
 * Returns { byChannel: { 1: [...], 2: [...], ... }, loading, error }.
 * Each item has: channel, timestamp, transmission_type, person_from, person_to, message, location, priority, ...
 * Normalized to exchange-like shape for TransmissionFeed: persona_name, timestamp_ms, transcript, response (optional).
 */
export function useSimulatedFeed() {
  const [byChannel, setByChannel] = useState({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    let cancelled = false;

    async function fetchSimulated() {
      try {
        const res = await fetch('/api/simulated/feed');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        const items = data.items || [];

        const grouped = {};
        for (const row of items) {
          const ch = row.channel;
          if (!grouped[ch]) grouped[ch] = [];
          grouped[ch].push(row);
        }
        for (const ch of Object.keys(grouped)) {
          grouped[ch].sort((a, b) => (a.timestamp || '').localeCompare(b.timestamp || ''));
          grouped[ch].reverse();
        }

        if (!cancelled) {
          setByChannel(grouped);
          setError(null);
        }
      } catch (e) {
        if (!cancelled) {
          setError(e.message || 'Failed to load simulated feed');
          setByChannel({});
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    }

    fetchSimulated();
    return () => { cancelled = true; };
  }, []);

  return { byChannel, loading, error };
}

/**
 * Normalize a simulated row to an exchange-like item for TransmissionCard.
 * Demo exchanges use: persona_name, timestamp_ms, transcript, response, language.
 * Simulated: we use person_from as persona_name, timestamp as timestamp_ms, message as transcript.
 */
export function simulatedRowToExchange(row) {
  const ts = row.timestamp || '';
  const ms = ts ? new Date(ts).getTime() : 0;
  return {
    persona_name: row.person_from || row.role_from || 'Unknown',
    timestamp_ms: ms,
    transcript: row.message || '',
    response: row.transmission_type === 'completion' || row.transmission_type === 'acknowledge' ? null : '',
    language: 'en',
    location: row.location,
    priority: row.priority,
    transmission_type: row.transmission_type,
  };
}
