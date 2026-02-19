import { T } from '../../theme';

export default function Card({ children, title, sub, span }) {
  return (
    <div
      style={{
        background: T.card,
        border: `1px solid ${T.border}`,
        borderRadius: 10,
        padding: '18px 22px',
        gridColumn: span ? `span ${span}` : undefined,
      }}
    >
      {title && (
        <div
          style={{
            fontSize: 11,
            fontWeight: 600,
            color: T.dim,
            textTransform: 'uppercase',
            letterSpacing: '0.08em',
            marginBottom: sub ? 2 : 14,
          }}
        >
          {title}
        </div>
      )}
      {sub && (
        <div style={{ fontSize: 10, color: T.muted, marginBottom: 14 }}>{sub}</div>
      )}
      {children}
    </div>
  );
}
