import {
  AreaChart,
  Area,
  Cell,
  CartesianGrid,
  Legend,
  Pie,
  PieChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts';
import Card from '../ui/Card';
import Metric from '../ui/Metric';
import ChannelBadge from '../ui/ChannelBadge';
import Tip from '../ui/Tip';
import Bar2 from '../ui/Bar2';
import { T, CH_COLORS } from '../../theme';
import { CH, HOURLY_TOTAL, STRESS, TOTALS } from '../../constants/channelMeta';

export default function Home() {
  const chList = Object.entries(CH).sort((a, b) => b[1].tx - a[1].tx);
  const maxTx = chList[0]?.[1]?.tx ?? 1;
  const hourlyData = Object.entries(HOURLY_TOTAL).map(([h, v]) => ({
    h: `${String(h).padStart(2, '0')}:00`,
    v,
  }));

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 14 }}>
      <Card title="Total Transmissions" sub="14-day capture window">
        <Metric value={TOTALS.tx.toLocaleString()} label="PTT Events Recorded" />
      </Card>
      <Card title="Total Air Time" sub="Cumulative channel usage">
        <Metric value={TOTALS.airMin.toLocaleString()} unit=" min" label="Radio Active" color={T.blue} />
      </Card>
      <Card title="Stress Windows" sub="15-min windows with 5+ channels active">
        <Metric
          value={`${STRESS.pct}%`}
          unit=""
          label={`${STRESS.windows} of ${STRESS.total} windows`}
          color={T.red}
        />
      </Card>
      <Card title="Coverage Gaps" sub="Dead air events (>15 min silence)">
        <Metric value={TOTALS.deadAir.toLocaleString()} label="Across All Channels" color={T.yellow} />
      </Card>

      <Card title="All-Channel Activity" sub="Transmissions per hour (14-day aggregate)" span={4}>
        <ResponsiveContainer width="100%" height={180}>
          <AreaChart data={hourlyData}>
            <defs>
              <linearGradient id="gAll" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor={T.orange} stopOpacity={0.35} />
                <stop offset="100%" stopColor={T.orange} stopOpacity={0} />
              </linearGradient>
            </defs>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="h" tick={{ fill: T.muted, fontSize: 9 }} interval={1} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} />
            <Tooltip content={<Tip />} />
            <Area
              type="monotone"
              dataKey="v"
              name="Transmissions"
              stroke={T.orange}
              fill="url(#gAll)"
              strokeWidth={2.5}
              dot={false}
            />
          </AreaChart>
        </ResponsiveContainer>
      </Card>

      <Card title="Channel Activity Ranking" sub="Daily average transmissions" span={2}>
        {chList.map(([ch, d]) => (
          <div key={ch} style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 10 }}>
            <ChannelBadge ch={Number(ch)} />
            <div style={{ flex: 1 }}>
              <Bar2 label={d.short} value={d.daily} max={maxTx / 14} color={CH_COLORS[ch]} sub="/day" />
            </div>
          </div>
        ))}
      </Card>

      <Card title="Channel Share" sub="% of total transmissions" span={2}>
        <ResponsiveContainer width="100%" height={260}>
          <PieChart>
            <Pie
              data={chList.map(([ch, d]) => ({ name: `CH${ch} ${d.short}`, value: d.tx, fill: CH_COLORS[ch] }))}
              dataKey="value"
              nameKey="name"
              cx="50%"
              cy="50%"
              outerRadius={95}
              innerRadius={50}
              paddingAngle={2}
              strokeWidth={0}
            >
              {chList.map(([ch]) => (
                <Cell key={ch} fill={CH_COLORS[ch]} />
              ))}
            </Pie>
            <Tooltip content={<Tip />} />
            <Legend wrapperStyle={{ fontSize: 10, color: T.dim }} />
          </PieChart>
        </ResponsiveContainer>
      </Card>

      <div style={{ gridColumn: 'span 4', display: 'grid', gridTemplateColumns: 'repeat(7, 1fr)', gap: 8 }}>
        {Object.entries(CH).map(([ch, d]) => (
          <div
            key={ch}
            style={{
              background: T.surface,
              border: `1px solid ${CH_COLORS[ch]}25`,
              borderRadius: 8,
              padding: '12px 10px',
              borderTop: `2px solid ${CH_COLORS[ch]}`,
            }}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
              <span
                style={{
                  fontSize: 10,
                  fontWeight: 700,
                  color: CH_COLORS[ch],
                  fontFamily: "'Space Mono', monospace",
                }}
              >
                CH{ch}
              </span>
              <span
                style={{
                  width: 6,
                  height: 6,
                  borderRadius: '50%',
                  background: T.green,
                  transition: 'background 0.5s',
                }}
              />
            </div>
            <div style={{ fontSize: 10, color: T.dim, marginBottom: 4 }}>{d.short}</div>
            <div
              style={{
                fontSize: 18,
                fontWeight: 800,
                color: T.text,
                fontFamily: "'Space Mono', monospace",
                lineHeight: 1,
              }}
            >
              {d.daily}
            </div>
            <div style={{ fontSize: 9, color: T.muted }}>tx/day</div>
            <div style={{ marginTop: 8, fontSize: 9, color: T.muted, display: 'flex', justifyContent: 'space-between' }}>
              <span>Cadence</span>
              <span style={{ color: T.dim, fontFamily: "'Space Mono', monospace" }}>
                {Math.round(d.medCadence / 60)}m
              </span>
            </div>
            <div style={{ fontSize: 9, color: T.muted, display: 'flex', justifyContent: 'space-between' }}>
              <span>Dead Air</span>
              <span
                style={{
                  color: d.deadAir > 350 ? T.red : T.dim,
                  fontFamily: "'Space Mono', monospace",
                }}
              >
                {d.deadAir}
              </span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
