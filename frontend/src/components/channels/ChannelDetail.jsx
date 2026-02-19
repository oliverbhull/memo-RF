import { useState } from 'react';
import {
  AreaChart,
  Area,
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import Card from '../ui/Card';
import Metric from '../ui/Metric';
import Bar2 from '../ui/Bar2';
import Tip from '../ui/Tip';
import { T, CH_COLORS } from '../../theme';
import { CH, CH_HOURLY, CH_DAILY } from '../../constants/channelMeta';

export default function ChannelDetail() {
  const [sel, setSel] = useState(1);
  const d = CH[sel];
  const color = CH_COLORS[sel];

  const hourly = Object.entries(CH_HOURLY[sel]).map(([h, v]) => ({
    h: String(h).padStart(2, '0'),
    v,
  }));
  const daily = Object.entries(CH_DAILY[sel]).map(([day, v]) => ({ d: day, v }));

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 14 }}>
      <div style={{ gridColumn: 'span 4', display: 'flex', gap: 6, flexWrap: 'wrap' }}>
        {Object.entries(CH).map(([ch, cd]) => (
          <button
            key={ch}
            onClick={() => setSel(Number(ch))}
            style={{
              padding: '8px 14px',
              borderRadius: 6,
              border: `1px solid ${Number(ch) === sel ? CH_COLORS[ch] : T.border}`,
              background: Number(ch) === sel ? `${CH_COLORS[ch]}15` : T.surface,
              cursor: 'pointer',
              color: Number(ch) === sel ? CH_COLORS[ch] : T.dim,
              fontSize: 12,
              fontWeight: 600,
              fontFamily: "'Space Mono', monospace",
            }}
          >
            CH{ch} {cd.short}
          </button>
        ))}
      </div>

      <Card title={`CH${sel} · ${d.name}`} sub="Channel Performance Summary">
        <Metric value={d.daily} unit="/day" label="Avg Transmissions" color={color} size={32} />
        <div style={{ marginTop: 16 }}>
          <Bar2 label="Avg Duration" value={d.avgDur} max={10} color={color} sub="s" />
          <Bar2 label="Med Cadence" value={Math.round(d.medCadence / 60)} max={25} color={color} sub=" min" />
          <Bar2 label="Utilization" value={d.util} max={1.5} color={color} sub="%" />
          <Bar2
            label="Dead Air"
            value={d.deadAir}
            max={450}
            color={d.deadAir > 350 ? T.red : color}
            sub=" events"
          />
        </div>
      </Card>

      <Card title="Hourly Distribution" sub="Transmissions by hour of day" span={2}>
        <ResponsiveContainer width="100%" height={220}>
          <BarChart data={hourly}>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="h" tick={{ fill: T.muted, fontSize: 9 }} interval={1} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} />
            <Tooltip content={<Tip />} />
            <Bar dataKey="v" name="Transmissions" radius={[3, 3, 0, 0]}>
              {hourly.map((e, i) => (
                <Cell key={i} fill={e.v > (d.tx / 24) * 1.3 ? color : `${color}60`} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <Card title="Transmission Profile" sub="Duration breakdown">
        <div style={{ padding: '10px 0' }}>
          {[
            { label: 'Short (<3s)', pct: d.shortPct, desc: 'Quick acks, confirmations', color: T.green },
            { label: 'Medium (3-10s)', pct: d.medPct, desc: 'Standard requests, updates', color: T.blue },
            { label: 'Long (>10s)', pct: d.longPct, desc: 'Complex requests, confusion', color: T.red },
          ].map((t) => (
            <div key={t.label} style={{ marginBottom: 14 }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 11, marginBottom: 4 }}>
                <span style={{ color: T.text }}>{t.label}</span>
                <span
                  style={{
                    color: t.color,
                    fontFamily: "'Space Mono', monospace",
                    fontWeight: 700,
                  }}
                >
                  {t.pct}%
                </span>
              </div>
              <div style={{ height: 8, background: T.bg, borderRadius: 4, overflow: 'hidden' }}>
                <div
                  style={{
                    width: `${t.pct}%`,
                    height: '100%',
                    background: t.color,
                    borderRadius: 4,
                  }}
                />
              </div>
              <div style={{ fontSize: 9, color: T.muted, marginTop: 2 }}>{t.desc}</div>
            </div>
          ))}
        </div>
      </Card>

      <Card title="Daily Volume Trend" sub="Transmissions per day" span={4}>
        <ResponsiveContainer width="100%" height={160}>
          <AreaChart data={daily}>
            <defs>
              <linearGradient id={`gCh${sel}`} x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor={color} stopOpacity={0.3} />
                <stop offset="100%" stopColor={color} stopOpacity={0} />
              </linearGradient>
            </defs>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="d" tick={{ fill: T.muted, fontSize: 10 }} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} />
            <Tooltip content={<Tip />} />
            <Area
              type="monotone"
              dataKey="v"
              name="Transmissions"
              stroke={color}
              fill={`url(#gCh${sel})`}
              strokeWidth={2}
              dot={{ fill: color, r: 2.5 }}
            />
          </AreaChart>
        </ResponsiveContainer>
      </Card>
    </div>
  );
}
