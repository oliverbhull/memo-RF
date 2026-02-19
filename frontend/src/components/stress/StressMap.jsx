import React from 'react';
import { Bar, BarChart, CartesianGrid, Legend, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import Card from '../ui/Card';
import Metric from '../ui/Metric';
import Tip from '../ui/Tip';
import { T, CH_COLORS } from '../../theme';
import { CH, CH_HOURLY, WEEKLY } from '../../constants/channelMeta';

const days = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
const dayKeys = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];
const hours = Array.from({ length: 24 }, (_, i) => i);

export default function StressMap() {
  let maxV = 0;
  dayKeys.forEach((d) =>
    hours.forEach((h) => {
      const v = WEEKLY[d]?.[h] || 0;
      if (v > maxV) maxV = v;
    })
  );

  const stackedData = hours.map((h) => {
    const row = { h: `${String(h).padStart(2, '0')}:00` };
    Object.entries(CH_HOURLY).forEach(([ch, hData]) => {
      row[`ch${ch}`] = hData[h] || 0;
    });
    return row;
  });

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr', gap: 14 }}>
      <Card
        title="Weekly Stress Heatmap"
        sub="All-channel volume by day of week and hour - identifies operational pressure points"
      >
        <div style={{ overflowX: 'auto', paddingBottom: 8 }}>
          <div
            style={{
              display: 'grid',
              gridTemplateColumns: '50px repeat(24, 1fr)',
              gap: 2,
              minWidth: 700,
            }}
          >
            <div />
            {hours.map((h) => (
              <div
                key={h}
                style={{
                  textAlign: 'center',
                  color: T.muted,
                  fontSize: 9,
                  paddingBottom: 4,
                  fontFamily: "'Space Mono', monospace",
                }}
              >
                {h}
              </div>
            ))}
            {dayKeys.map((day, di) => (
              <React.Fragment key={day}>
                <div
                  style={{
                    color: T.dim,
                    fontSize: 11,
                    display: 'flex',
                    alignItems: 'center',
                    fontWeight: 600,
                  }}
                >
                  {days[di]}
                </div>
                {hours.map((h) => {
                  const val = WEEKLY[day]?.[h] || 0;
                  const intensity = maxV > 0 ? val / maxV : 0;
                  const isHot = intensity > 0.7;
                  return (
                    <div
                      key={`${day}-${h}`}
                      style={{
                        background: val === 0 ? T.bg : `rgba(255,107,43,${intensity * 0.85})`,
                        borderRadius: 2,
                        height: 28,
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        color: isHot ? '#fff' : intensity > 0.3 ? T.text : T.muted,
                        fontSize: 9,
                        fontFamily: "'Space Mono', monospace",
                        fontWeight: isHot ? 700 : 400,
                        border: isHot ? `1px solid ${T.orange}60` : '1px solid transparent',
                      }}
                    >
                      {val > 0 ? val : ''}
                    </div>
                  );
                })}
              </React.Fragment>
            ))}
          </div>
        </div>
      </Card>

      <Card title="Channel Stacked Activity" sub="Which channels contribute to each hour's volume">
        <ResponsiveContainer width="100%" height={260}>
          <BarChart data={stackedData}>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="h" tick={{ fill: T.muted, fontSize: 9 }} interval={1} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} />
            <Tooltip content={<Tip />} />
            <Legend wrapperStyle={{ fontSize: 10 }} />
            {Object.keys(CH).map((ch) => (
              <Bar key={ch} dataKey={`ch${ch}`} name={CH[ch].short} stackId="a" fill={CH_COLORS[ch]} />
            ))}
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 14 }}>
        <Card title="Peak Hour" sub="Highest single-hour volume">
          <Metric value="11:00" unit="" label="504 transmissions" color={T.red} size={28} />
        </Card>
        <Card title="Quiet Hour" sub="Lowest active-hours volume">
          <Metric value="05:00" unit="" label="48 transmissions" color={T.green} size={28} />
        </Card>
        <Card title="Busiest Day" sub="Highest daily pattern">
          <Metric value="Saturday" unit="" label="Avg 173 tx/hr peak" color={T.orange} size={28} />
        </Card>
      </div>
    </div>
  );
}
