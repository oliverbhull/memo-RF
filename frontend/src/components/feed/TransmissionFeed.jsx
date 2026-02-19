import { T } from '../../theme';
import TransmissionCard from './TransmissionCard';

/**
 * List of transmission cards. items = array of exchange-like or simulated row objects.
 * channelId: 0 = Demo, 1-7 = CH1-CH7 (for label/empty state).
 */
export default function TransmissionFeed({ items = [], channelId, emptyMessage }) {
  const defaultEmpty =
    channelId === 0
      ? 'No transmissions yet. Speak into the radio...'
      : `No simulated transmissions for this channel.`;
  const msg = emptyMessage ?? defaultEmpty;

  if (!items.length) {
    return (
      <div
        style={{
          textAlign: 'center',
          padding: '60px 20px',
          color: T.dim,
          fontSize: '1.2em',
        }}
      >
        {msg}
      </div>
    );
  }

  return (
    <div>
      {items.map((item, i) => (
        <TransmissionCard key={i} item={item} />
      ))}
    </div>
  );
}
