import { CH_COLORS } from '../../theme';
import { T } from '../../theme';

export default function ChannelBadge({ ch }) {
  const color = ch === 0 ? T.cyan : CH_COLORS[ch];
  const label = ch === 0 ? 'Demo' : `CH${ch}`;
  return (
    <span
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        gap: 5,
        padding: '2px 8px',
        borderRadius: 4,
        background: `${color}15`,
        border: `1px solid ${color}30`,
        fontSize: 11,
        color,
        fontWeight: 600,
        fontFamily: "'Space Mono', monospace",
      }}
    >
      {label}
    </span>
  );
}
