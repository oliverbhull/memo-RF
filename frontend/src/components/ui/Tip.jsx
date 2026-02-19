import { T } from '../../theme';

export default function Tip({ active, payload, label }) {
  if (!active || !payload?.length) return null;
  return (
    <div
      style={{
        background: T.card,
        border: `1px solid ${T.border}`,
        borderRadius: 6,
        padding: '6px 10px',
        fontSize: 11,
      }}
    >
      <div style={{ color: T.dim, marginBottom: 3 }}>{label}</div>
      {payload.map((p, i) => (
        <div
          key={i}
          style={{
            color: p.color || T.text,
            fontFamily: "'Space Mono', monospace",
            fontSize: 11,
          }}
        >
          {p.name}: {typeof p.value === 'number' ? Math.round(p.value * 10) / 10 : p.value}
        </div>
      ))}
    </div>
  );
}
