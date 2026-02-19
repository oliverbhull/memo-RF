import { Bar, BarChart, CartesianGrid, Cell, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import Card from '../ui/Card';
import ChannelBadge from '../ui/ChannelBadge';
import Bar2 from '../ui/Bar2';
import Tip from '../ui/Tip';
import { T, CH_COLORS } from '../../theme';
import { CH, CH_HOURLY } from '../../constants/channelMeta';

export default function CoverageGaps() {
  const gapData = Object.entries(CH)
    .map(([ch, d]) => ({
      ch: Number(ch),
      name: `CH${ch} ${d.short}`,
      deadAir: d.deadAir,
      cadence: Math.round(d.cadence / 60),
      medCadence: Math.round(d.medCadence / 60),
      color: CH_COLORS[ch],
    }))
    .sort((a, b) => b.deadAir - a.deadAir);

  const maxDead = gapData[0]?.deadAir ?? 1;

  const overnightChannels = Object.entries(CH_HOURLY)
    .filter(([ch]) => Number(ch) !== 4)
    .map(([ch, h]) => {
      const nightTx =
        (h[0] || 0) +
        (h[1] || 0) +
        (h[2] || 0) +
        (h[3] || 0) +
        (h[4] || 0) +
        (h[5] || 0);
      return { ch: Number(ch), name: CH[ch].short, nightTx, color: CH_COLORS[ch] };
    })
    .sort((a, b) => a.nightTx - b.nightTx);

  const cadenceSorted = [...gapData].sort((a, b) => b.medCadence - a.medCadence);

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 14 }}>
      <Card
        title="Dead Air Events by Channel"
        sub="Gaps >15 min during active hours — indicates staffing holes"
      >
        {gapData.map((d) => (
          <div key={d.ch} style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 8 }}>
            <ChannelBadge ch={d.ch} />
            <div style={{ flex: 1 }}>
              <Bar2
                label={d.name.split(' ').slice(1).join(' ')}
                value={d.deadAir}
                max={maxDead}
                color={d.deadAir > 350 ? T.red : d.color}
                sub=""
              />
            </div>
          </div>
        ))}
      </Card>

      <Card title="Response Cadence" sub="Median time between consecutive transmissions on same channel">
        <ResponsiveContainer width="100%" height={260}>
          <BarChart data={cadenceSorted}>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="name" tick={{ fill: T.muted, fontSize: 9 }} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} unit="m" />
            <Tooltip content={<Tip />} />
            <Bar dataKey="medCadence" name="Median Cadence (min)" radius={[4, 4, 0, 0]}>
              {cadenceSorted.map((d, i) => (
                <Cell key={i} fill={d.medCadence > 15 ? T.yellow : d.color} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <Card
        title="Overnight Activity"
        sub="Transmissions between 12am-6am (excluding Security)"
        span={2}
      >
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 10 }}>
          {overnightChannels.map((d) => (
            <div
              key={d.ch}
              style={{
                padding: '14px 16px',
                background: T.surface,
                borderRadius: 8,
                border: `1px solid ${d.nightTx < 15 ? `${T.red}40` : T.border}`,
                borderLeft: `3px solid ${d.nightTx < 15 ? T.red : d.color}`,
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div>
                  <div style={{ fontSize: 10, color: T.dim }}>
                    <ChannelBadge ch={d.ch} /> {d.name}
                  </div>
                  <div style={{ fontSize: 10, color: T.muted, marginTop: 4 }}>
                    {d.nightTx < 15
                      ? '⚠ Near-silent overnight'
                      : d.nightTx < 30
                        ? 'Low overnight activity'
                        : 'Active overnight'}
                  </div>
                </div>
                <div
                  style={{
                    fontSize: 24,
                    fontWeight: 800,
                    color: d.nightTx < 15 ? T.red : T.text,
                    fontFamily: "'Space Mono', monospace",
                  }}
                >
                  {d.nightTx}
                </div>
              </div>
            </div>
          ))}
        </div>
        <div
          style={{
            marginTop: 12,
            padding: '10px 14px',
            background: `${T.red}08`,
            border: `1px solid ${T.red}20`,
            borderRadius: 6,
            fontSize: 11,
            color: T.dim,
          }}
        >
          Channels with near-zero overnight activity represent coverage blind spots. If a guest calls
          front desk at 2am about a plumbing emergency, is engineering's radio monitored?
        </div>
      </Card>
    </div>
  );
}
