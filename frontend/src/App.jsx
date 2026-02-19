import { useState } from 'react';
import { T } from './theme';
import { TABS } from './constants/tabs';
import LiveOverview from './components/live/LiveOverview';
import ChannelDetail from './components/channels/ChannelDetail';
import StressMap from './components/stress/StressMap';
import CoverageGaps from './components/coverage/CoverageGaps';
import SignalDuration from './components/signal/SignalDuration';
import { TOTALS } from './constants/channelMeta';

const TAB_MAP = {
  live: LiveOverview,
  channels: ChannelDetail,
  stress: StressMap,
  coverage: CoverageGaps,
  signal: SignalDuration,
};

export default function App() {
  const [tab, setTab] = useState('live');
  const View = TAB_MAP[tab];

  return (
    <div
      style={{
        background: T.bg,
        minHeight: '100vh',
        color: T.text,
        fontFamily: "'Inter', -apple-system, sans-serif",
      }}
    >
      <link
        href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&family=Space+Mono:wght@400;700&display=swap"
        rel="stylesheet"
      />

      <div
        style={{
          padding: '16px 28px',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          borderBottom: `1px solid ${T.border}`,
          background: `linear-gradient(180deg, ${T.card} 0%, ${T.bg} 100%)`,
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
          <div
            style={{
              width: 32,
              height: 32,
              borderRadius: 8,
              background: `linear-gradient(135deg, ${T.orange}, ${T.red})`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: 14,
              fontWeight: 800,
              color: '#fff',
            }}
          >
            M
          </div>
          <div>
            <div style={{ fontSize: 16, fontWeight: 800, letterSpacing: '-0.02em' }}>
              Memo <span style={{ color: T.orange }}>Radio Intelligence</span>
            </div>
            <div style={{ fontSize: 10, color: T.muted }}>
              Fairmont Hotel · Channel Monitor · Jan 6–19, 2025
            </div>
          </div>
        </div>
        <div style={{ display: 'flex', gap: 20, fontSize: 11, color: T.dim }}>
          <span>
            <span style={{ fontFamily: "'Space Mono', monospace", color: T.text, fontWeight: 700 }}>7</span> channels
          </span>
          <span>
            <span style={{ fontFamily: "'Space Mono', monospace", color: T.text, fontWeight: 700 }}>
              {TOTALS.tx.toLocaleString()}
            </span>{' '}
            PTT events
          </span>
          <span>
            <span style={{ fontFamily: "'Space Mono', monospace", color: T.text, fontWeight: 700 }}>14</span> days
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
            <span
              style={{
                width: 6,
                height: 6,
                borderRadius: '50%',
                background: T.green,
                boxShadow: `0 0 6px ${T.green}`,
              }}
            />
            <span style={{ color: T.green, fontWeight: 600 }}>MONITORING</span>
          </span>
        </div>
      </div>

      <div
        style={{
          display: 'flex',
          gap: 0,
          borderBottom: `1px solid ${T.border}`,
          padding: '0 28px',
          overflowX: 'auto',
        }}
      >
        {TABS.map((t) => (
          <button
            key={t.id}
            onClick={() => setTab(t.id)}
            style={{
              padding: '11px 18px',
              fontSize: 12,
              fontWeight: tab === t.id ? 700 : 400,
              color: tab === t.id ? T.orange : T.muted,
              background: 'none',
              border: 'none',
              cursor: 'pointer',
              borderBottom: tab === t.id ? `2px solid ${T.orange}` : '2px solid transparent',
              fontFamily: "'Inter', sans-serif",
              display: 'flex',
              alignItems: 'center',
              gap: 6,
              whiteSpace: 'nowrap',
            }}
          >
            <span style={{ fontSize: 10 }}>{t.icon}</span> {t.label}
          </button>
        ))}
      </div>

      <div style={{ padding: '20px 28px', maxWidth: 1200 }}>
        <View />
      </div>
    </div>
  );
}
