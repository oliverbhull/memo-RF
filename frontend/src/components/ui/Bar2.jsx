import { T } from '../../theme';

export default function Bar2({ label, value, max, color, sub }) {
  const pct = max > 0 ? (value / max) * 100 : 0;
  return (
    <div style={{ marginBottom: 10 }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 3 }}>
        <span style={{ fontSize: 12, color: T.text }}>{label}</span>
        <span
          style={{
            fontSize: 12,
            color,
            fontFamily: "'Space Mono', monospace",
            fontWeight: 600,
          }}
        >
          {value}
          {sub}
        </span>
      </div>
      <div style={{ height: 6, background: T.bg, borderRadius: 3, overflow: 'hidden' }}>
        <div
          style={{
            width: `${Math.min(pct, 100)}%`,
            height: '100%',
            background: `linear-gradient(90deg, ${color}, ${color}aa)`,
            borderRadius: 3,
            transition: 'width 0.6s cubic-bezier(0.16,1,0.3,1)',
          }}
        />
      </div>
    </div>
  );
}
