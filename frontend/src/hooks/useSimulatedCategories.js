import { useState, useEffect } from 'react';

/**
 * Fetch GET /api/simulated/categories.
 * Returns { byChannel: { "1": { extra_towels: 45, ... }, ... }, loading, error }.
 */
export function useSimulatedCategories() {
  const [byChannel, setByChannel] = useState({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    let cancelled = false;

    async function fetchCategories() {
      try {
        const res = await fetch('/api/simulated/categories');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        if (!cancelled) {
          setByChannel(typeof data === 'object' && data !== null ? data : {});
          setError(null);
        }
      } catch (e) {
        if (!cancelled) {
          setError(e.message || 'Failed to load categories');
          setByChannel({});
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    }

    fetchCategories();
    return () => { cancelled = true; };
  }, []);

  return { byChannel, loading, error };
}
