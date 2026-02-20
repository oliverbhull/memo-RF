import { useState, useEffect } from 'react';

/**
 * Fetch GET /api/simulated/insights?channel=X.
 * Returns { insights: [{ id, text, subtext }], loading, error }.
 */
export function useSimulatedInsights(channel) {
  const [insights, setInsights] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    if (channel == null) {
      setInsights([]);
      setLoading(false);
      return;
    }
    let cancelled = false;

    async function fetchInsights() {
      setLoading(true);
      try {
        const res = await fetch(`/api/simulated/insights?channel=${encodeURIComponent(channel)}`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        if (!cancelled) {
          setInsights(Array.isArray(data.insights) ? data.insights : []);
          setError(null);
        }
      } catch (e) {
        if (!cancelled) {
          setError(e.message || 'Failed to load insights');
          setInsights([]);
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    }

    fetchInsights();
    return () => { cancelled = true; };
  }, [channel]);

  return { insights, loading, error };
}
