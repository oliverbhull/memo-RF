import { useState, useEffect } from 'react';

const POLL_MS = 2000;

/**
 * Poll GET /api/feed every 2s. Returns { exchanges, loading, error }.
 * Used for the live "Demo" channel.
 */
export function useDemoFeed() {
  const [exchanges, setExchanges] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    let cancelled = false;

    async function fetchFeed() {
      try {
        const res = await fetch('/api/feed');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        if (!cancelled) {
          setExchanges(data.exchanges || []);
          setError(null);
        }
      } catch (e) {
        if (!cancelled) {
          setError(e.message || 'Failed to load feed');
          setExchanges([]);
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    }

    fetchFeed();
    const id = setInterval(fetchFeed, POLL_MS);
    return () => {
      cancelled = true;
      clearInterval(id);
    };
  }, []);

  return { exchanges, loading, error };
}
