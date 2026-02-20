import { T } from '../../theme';

/**
 * Renders a single transmission. Accepts either:
 * - Demo exchange: { persona_name, timestamp_ms, transcript, response, language }
 * - Simulated row: { person_from, message, timestamp, transmission_type, location }
 */
export default function TransmissionCard({ item }) {
  const isDemo = 'transcript' in item && 'persona_name' in item;
  const persona = isDemo ? (item.persona_name || 'Unknown') : (item.person_from || item.role_from || 'Unknown');
  const ts = isDemo ? item.timestamp_ms : (item.timestamp ? new Date(item.timestamp).getTime() : 0);
  const time = ts ? new Date(ts).toLocaleTimeString() : '';
  const transcript = isDemo ? item.transcript : item.message;
  const response = isDemo ? item.response : null;
  const langCode = isDemo && item.language ? (item.language === 'en' ? 'EN' : item.language.toUpperCase().slice(0, 2)) : null;
  const location = item.location || (isDemo ? 'Fort Mason, Bldg C Floor 2N' : null);

  return (
    <div
      style={{
        background: T.card,
        border: `1px solid ${T.border}`,
        borderRadius: 8,
        padding: 15,
        marginBottom: 15,
        transition: 'border-color 0.2s',
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          marginBottom: 10,
          paddingBottom: 8,
          borderBottom: `1px solid ${T.border}`,
        }}
      >
        <div>
          <span style={{ color: T.orange, fontWeight: 'bold' }}>{persona}</span>
          {item.channel != null && item.channel >= 1 && item.channel <= 7 && (
            <span
              style={{
                display: 'inline-block',
                background: T.surface,
                color: T.cyan,
                padding: '2px 6px',
                borderRadius: 8,
                fontSize: '0.7em',
                marginLeft: 6,
                fontWeight: 600,
              }}
            >
              CH{item.channel}
            </span>
          )}
          {langCode && (
            <span
              style={{
                display: 'inline-block',
                background: T.surface,
                color: T.blue,
                padding: '2px 8px',
                borderRadius: 12,
                fontSize: '0.75em',
                marginLeft: 8,
                fontWeight: 600,
              }}
            >
              {langCode}
            </span>
          )}
          {location && (
            <span
              style={{
                display: 'inline-block',
                background: T.surface,
                color: T.dim,
                padding: '2px 8px',
                borderRadius: 12,
                fontSize: '0.7em',
                marginLeft: 8,
              }}
            >
              {location}
            </span>
          )}
        </div>
        <span style={{ color: T.dim, fontSize: '0.85em' }}>{time}</span>
      </div>
      <div
        style={{
          background: T.bg,
          padding: 15,
          borderLeft: `3px solid ${T.border}`,
          borderRadius: 4,
          lineHeight: 1.6,
        }}
      >
        {transcript && (
          <div>
            <span style={{ color: T.blue, fontWeight: 'bold', marginRight: 5 }}>&gt; </span>
            {transcript}
          </div>
        )}
        {response && (
          <div style={{ marginTop: 10 }}>
            <span style={{ color: T.red, fontWeight: 'bold', marginRight: 5 }}>&lt; </span>
            {response}
          </div>
        )}
      </div>
    </div>
  );
}
