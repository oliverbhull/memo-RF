import { T } from '../../theme';

export default function Metric({ value, unit, label, color = T.orange, size = 38 }) {
  return (
    <div style={{ textAlign: 'center' }}>
      <div
        style={{
          fontSize: size,
          fontWeight: 800,
          color,
          fontFamily: "'Space Mono', monospace",
          lineHeight: 1,
          letterSpacing: '-0.03em',
        }}
      >
        {value}
        {unit && (
          <span style={{ fontSize: size * 0.4, color: T.muted, fontWeight: 400 }}>{unit}</span>
        )}
      </div>
      {label && (
        <div
          style={{
            fontSize: 10,
            color: T.muted,
            marginTop: 6,
            textTransform: 'uppercase',
            letterSpacing: '0.08em',
          }}
        >
          {label}
        </div>
      )}
    </div>
  );
}
