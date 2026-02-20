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
import { useSimulatedCategories } from '../../hooks/useSimulatedCategories';
import { useSimulatedInsights } from '../../hooks/useSimulatedInsights';
import { getRequestCategoryLabel, isCategoryKey } from '../../constants/requestCategories';
import { T, CH_COLORS } from '../../theme';
import { CH, CH_HOURLY, CH_DAILY } from '../../constants/channelMeta';

export default function ChannelDetail() {
  const [sel, setSel] = useState(1);
  const d = CH[sel];
  const color = CH_COLORS[sel];
  const { byChannel, loading: categoriesLoading, error: categoriesError } = useSimulatedCategories();
  const { insights, loading: insightsLoading, error: insightsError } = useSimulatedInsights(sel);

  const categoryCounts = byChannel[String(sel)] || {};
  const requestBreakdownData = Object.entries(categoryCounts)
    .filter(([key]) => isCategoryKey(key))
    .map(([key, count]) => ({ name: getRequestCategoryLabel(key), key, count }))
    .sort((a, b) => b.count - a.count);

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

      <Card title="Request breakdown" sub="Request types on this channel (14-day)" span={2}>
        {categoriesLoading && <div style={{ color: T.dim, fontSize: 12 }}>Loading...</div>}
        {categoriesError && <div style={{ color: T.red, fontSize: 12 }}>{categoriesError}</div>}
        {!categoriesLoading && !categoriesError && requestBreakdownData.length === 0 && (
          <div style={{ color: T.dim, fontSize: 12 }}>No category data for this channel.</div>
        )}
        {!categoriesLoading && !categoriesError && requestBreakdownData.length > 0 && (
          <ResponsiveContainer width="100%" height={Math.max(220, requestBreakdownData.length * 24)}>
            <BarChart data={requestBreakdownData} layout="vertical" margin={{ left: 4, right: 8 }}>
              <CartesianGrid strokeDasharray="3 3" stroke={T.border} horizontal={false} />
              <XAxis type="number" tick={{ fill: T.muted, fontSize: 9 }} />
              <YAxis type="category" dataKey="name" tick={{ fill: T.dim, fontSize: 10 }} width={140} />
              <Tooltip content={<Tip />} />
              <Bar dataKey="count" name="Requests" radius={[0, 4, 4, 0]}>
                {requestBreakdownData.map((_, i) => (
                  <Cell key={i} fill={color} />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        )}
      </Card>

      <Card title="Insights" sub="Intelligence derived from request data (14-day)" span={2}>
        {insightsLoading && <div style={{ color: T.dim, fontSize: 12 }}>Loading...</div>}
        {insightsError && <div style={{ color: T.red, fontSize: 12 }}>{insightsError}</div>}
        {!insightsLoading && !insightsError && insights.length === 0 && (
          <div style={{ color: T.dim, fontSize: 12 }}>No insights for this channel.</div>
        )}
        {!insightsLoading && !insightsError && insights.length > 0 && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
            {insights.map((item) => (
              <div
                key={item.id || item.text}
                style={{
                  padding: '10px 12px',
                  background: T.surface,
                  borderRadius: 8,
                  borderLeft: `3px solid ${color}`,
                }}
              >
                <div style={{ fontSize: 12, color: T.text, fontWeight: 600, lineHeight: 1.4 }}>
                  {item.text}
                </div>
                {item.subtext && (
                  <div style={{ fontSize: 11, color: T.muted, marginTop: 4 }}>
                    {item.subtext}
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
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
