import { useState } from 'react';
import Card from '../ui/Card';
import TransmissionFeed from '../feed/TransmissionFeed';
import { useDemoFeed } from '../../hooks/useDemoFeed';
import { useSimulatedFeed } from '../../hooks/useSimulatedFeed';
import { T, CH_COLORS } from '../../theme';
import { CH, DEMO_CHANNEL_LABEL } from '../../constants/channelMeta';

export default function LiveOverview() {
  const [feedChannel, setFeedChannel] = useState(0);
  const { exchanges: demoExchanges, loading: demoLoading, error: demoError } = useDemoFeed();
  const { byChannel: simulatedByChannel, loading: simLoading, error: simError } = useSimulatedFeed();

  const feedItems =
    feedChannel === 0 ? demoExchanges : (simulatedByChannel[feedChannel] || []);
  const feedLoading = feedChannel === 0 ? demoLoading : simLoading;
  const feedError = feedChannel === 0 ? demoError : simError;

  return (
    <div style={{ width: '100%' }}>
      <Card
        title="Live Feed"
        sub={feedChannel === 0 ? 'Demo channel — live from radio' : `CH${feedChannel} — simulated (14-day)`}
      >
        <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap', marginBottom: 14 }}>
          <button
            onClick={() => setFeedChannel(0)}
            style={{
              padding: '8px 14px',
              borderRadius: 6,
              border: `1px solid ${feedChannel === 0 ? T.cyan : T.border}`,
              background: feedChannel === 0 ? `${T.cyan}15` : T.surface,
              cursor: 'pointer',
              color: feedChannel === 0 ? T.cyan : T.dim,
              fontSize: 12,
              fontWeight: 600,
              fontFamily: "'Space Mono', monospace",
            }}
          >
            {DEMO_CHANNEL_LABEL}
          </button>
          {[1, 2, 3, 4, 5, 6, 7].map((ch) => (
            <button
              key={ch}
              onClick={() => setFeedChannel(ch)}
              style={{
                padding: '8px 14px',
                borderRadius: 6,
                border: `1px solid ${feedChannel === ch ? CH_COLORS[ch] : T.border}`,
                background: feedChannel === ch ? `${CH_COLORS[ch]}15` : T.surface,
                cursor: 'pointer',
                color: feedChannel === ch ? CH_COLORS[ch] : T.dim,
                fontSize: 12,
                fontWeight: 600,
                fontFamily: "'Space Mono', monospace",
              }}
            >
              CH{ch} {CH[ch].short}
            </button>
          ))}
        </div>
        {feedLoading && (
          <div style={{ color: T.dim, fontSize: 12, marginBottom: 10 }}>Loading...</div>
        )}
        {feedError && (
          <div style={{ color: T.red, fontSize: 12, marginBottom: 10 }}>{feedError}</div>
        )}
        <TransmissionFeed items={feedItems} channelId={feedChannel} />
      </Card>
    </div>
  );
}
