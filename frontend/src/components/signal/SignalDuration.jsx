import { Bar, BarChart, CartesianGrid, Cell, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import Card from '../ui/Card';
import Tip from '../ui/Tip';
import { T, CH_COLORS } from '../../theme';
import { CH } from '../../constants/channelMeta';

export default function SignalDuration() {
  const durData = Object.entries(CH)
    .map(([ch, d]) => ({
      ch: Number(ch),
      name: `CH${ch} ${d.short}`,
      avg: d.avgDur,
      med: d.medDur,
      max: d.maxDur,
      color: CH_COLORS[ch],
    }))
    .sort((a, b) => b.avg - a.avg);

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: 14 }}>
      <Card
        title="Avg Transmission Duration"
        sub="Longer = more complex requests or operational confusion"
      >
        <ResponsiveContainer width="100%" height={260}>
          <BarChart data={durData} layout="vertical">
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} horizontal={false} />
            <XAxis type="number" tick={{ fill: T.muted, fontSize: 9 }} unit="s" />
            <YAxis type="category" dataKey="name" tick={{ fill: T.dim, fontSize: 10 }} width={120} />
            <Tooltip content={<Tip />} />
            <Bar dataKey="avg" name="Avg Duration (s)" radius={[0, 4, 4, 0]}>
              {durData.map((d, i) => (
                <Cell key={i} fill={d.avg > 7 ? T.yellow : d.color} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <Card title="Duration Profile Comparison" sub="What type of transmissions dominate each channel">
        <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: 6, marginTop: 4 }}>
          {Object.entries(CH).map(([ch, d]) => (
            <div key={ch} style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 11 }}>
              <span
                style={{
                  width: 40,
                  color: CH_COLORS[ch],
                  fontFamily: "'Space Mono', monospace",
                  fontWeight: 700,
                  fontSize: 10,
                  flexShrink: 0,
                }}
              >
                CH{ch}
              </span>
              <div style={{ flex: 1, display: 'flex', height: 16, borderRadius: 3, overflow: 'hidden' }}>
                <div
                  style={{ width: `${d.shortPct}%`, background: T.green, transition: 'width 0.5s' }}
                  title={`Short: ${d.shortPct}%`}
                />
                <div
                  style={{ width: `${d.medPct}%`, background: T.blue, transition: 'width 0.5s' }}
                  title={`Medium: ${d.medPct}%`}
                />
                <div
                  style={{ width: `${d.longPct}%`, background: T.red, transition: 'width 0.5s' }}
                  title={`Long: ${d.longPct}%`}
                />
              </div>
              <span
                style={{
                  width: 35,
                  color: d.longPct > 20 ? T.red : T.muted,
                  fontFamily: "'Space Mono', monospace",
                  fontSize: 10,
                  textAlign: 'right',
                }}
              >
                {d.longPct}%
              </span>
            </div>
          ))}
        </div>
        <div style={{ display: 'flex', gap: 16, marginTop: 12, fontSize: 10, color: T.muted }}>
          <span>
            <span
              style={{
                display: 'inline-block',
                width: 8,
                height: 8,
                borderRadius: 2,
                background: T.green,
                marginRight: 4,
              }}
            />
            Short (&lt;3s)
          </span>
          <span>
            <span
              style={{
                display: 'inline-block',
                width: 8,
                height: 8,
                borderRadius: 2,
                background: T.blue,
                marginRight: 4,
              }}
            />
            Medium (3-10s)
          </span>
          <span>
            <span
              style={{
                display: 'inline-block',
                width: 8,
                height: 8,
                borderRadius: 2,
                background: T.red,
                marginRight: 4,
              }}
            />
            Long (&gt;10s)
          </span>
        </div>
        <div
          style={{
            marginTop: 12,
            padding: '10px 14px',
            background: `${T.yellow}08`,
            border: `1px solid ${T.yellow}20`,
            borderRadius: 6,
            fontSize: 11,
            color: T.dim,
          }}
        >
          Engineering (CH3) has 36.7% long transmissions — highest by far. Long transmissions often
          indicate complex issues being described verbally that would benefit from structured
          dispatch.
        </div>
      </Card>

      <Card title="Signal Quality" sub="Average signal strength and weak signal events" span={2}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(7, 1fr)', gap: 8 }}>
          {Object.entries(CH).map(([ch, d]) => (
            <div
              key={ch}
              style={{
                textAlign: 'center',
                padding: '12px 8px',
                background: T.surface,
                borderRadius: 8,
                border: `1px solid ${T.border}`,
              }}
            >
              <div
                style={{
                  fontSize: 10,
                  color: CH_COLORS[ch],
                  fontWeight: 700,
                  fontFamily: "'Space Mono', monospace",
                  marginBottom: 6,
                }}
              >
                CH{ch}
              </div>
              <div
                style={{
                  fontSize: 20,
                  fontWeight: 800,
                  color: T.text,
                  fontFamily: "'Space Mono', monospace",
                }}
              >
                {Math.round(d.signal)}
              </div>
              <div style={{ fontSize: 9, color: T.muted }}>dBm avg</div>
              <div
                style={{
                  fontSize: 10,
                  color: d.weakPct > 4 ? T.yellow : T.dim,
                  fontFamily: "'Space Mono', monospace",
                  marginTop: 4,
                }}
              >
                {d.weakPct}%
              </div>
              <div style={{ fontSize: 9, color: T.muted }}>weak signal</div>
            </div>
          ))}
        </div>
      </Card>
    </div>
  );
}
