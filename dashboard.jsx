import { useState, useEffect } from "react";
import { BarChart, Bar, LineChart, Line, XAxis, YAxis, Tooltip, ResponsiveContainer, Cell, PieChart, Pie, CartesianGrid, Legend, AreaChart, Area } from "recharts";

// ============================================================
// CHANNEL KPI DATA (from passive radio monitor simulation)
// ============================================================
const CH = {
  1: { name: "Front Desk / Ops", short: "Front Desk", tx: 1687, daily: 120.5, avgDur: 6.2, medDur: 5.7, maxDur: 42.6, airMin: 174.3, util: 1.22, cadence: 715.2, medCadence: 366, deadAir: 354, signal: -45.9, weakPct: 3.8, shortPct: 19.0, medPct: 70.1, longPct: 11.0 },
  2: { name: "Housekeeping", short: "Housekeeping", tx: 1171, daily: 83.6, avgDur: 5.1, medDur: 4.7, maxDur: 31.1, airMin: 99.8, util: 0.85, cadence: 1002.5, medCadence: 413, deadAir: 286, signal: -46.1, weakPct: 3.8, shortPct: 26.0, medPct: 69.1, longPct: 5.0 },
  3: { name: "Engineering", short: "Engineering", tx: 645, daily: 46.1, avgDur: 8.9, medDur: 8.2, maxDur: 45.0, airMin: 95.5, util: 0.76, cadence: 1826.7, medCadence: 873, deadAir: 312, signal: -45.7, weakPct: 4.7, shortPct: 15.7, medPct: 47.6, longPct: 36.7 },
  4: { name: "Security", short: "Security", tx: 831, daily: 59.4, avgDur: 4.6, medDur: 4.2, maxDur: 29.1, airMin: 63.5, util: 0.31, cadence: 1451.1, medCadence: 970, deadAir: 439, signal: -45.5, weakPct: 3.5, shortPct: 31.8, medPct: 64.0, longPct: 4.2 },
  5: { name: "F&B / Room Service", short: "F&B", tx: 889, daily: 63.5, avgDur: 5.4, medDur: 5.0, maxDur: 33.2, airMin: 80.0, util: 0.56, cadence: 1343.2, medCadence: 655, deadAir: 342, signal: -46.0, weakPct: 2.8, shortPct: 21.8, medPct: 73.7, longPct: 4.5 },
  6: { name: "Management / MOD", short: "Management", tx: 426, daily: 30.4, avgDur: 7.4, medDur: 7.2, maxDur: 36.5, airMin: 52.5, util: 0.39, cadence: 2814.2, medCadence: 1405, deadAir: 286, signal: -45.8, weakPct: 2.8, shortPct: 18.1, medPct: 58.0, longPct: 23.9 },
  7: { name: "Concierge / Bell", short: "Concierge", tx: 678, daily: 48.4, avgDur: 4.3, medDur: 4.2, maxDur: 18.5, airMin: 48.9, util: 0.39, cadence: 1736.5, medCadence: 863, deadAir: 327, signal: -46.0, weakPct: 3.4, shortPct: 27.6, medPct: 69.9, longPct: 2.5 },
};

const CH_HOURLY = {
  1: {0:9,1:10,2:9,3:7,4:10,5:6,6:83,7:53,8:96,9:83,10:154,11:143,12:68,13:87,14:71,15:91,16:139,17:117,18:100,19:89,20:81,21:100,22:72,23:9},
  2: {0:0,1:5,2:7,3:3,4:2,5:4,6:2,7:42,8:64,9:111,10:112,11:116,12:116,13:104,14:90,15:41,16:79,17:72,18:57,19:61,20:77,21:3,22:0,23:3},
  3: {0:0,1:4,2:2,3:3,4:0,5:2,6:3,7:16,8:38,9:58,10:50,11:61,12:35,13:48,14:73,15:46,16:38,17:30,18:31,19:31,20:35,21:35,22:5,23:1},
  4: {0:43,1:49,2:32,3:29,4:26,5:28,6:33,7:26,8:23,9:35,10:25,11:40,12:35,13:31,14:24,15:18,16:29,17:57,18:49,19:38,20:27,21:29,22:61,23:44},
  5: {0:2,1:3,2:2,3:4,4:5,5:1,6:45,7:44,8:68,9:54,10:49,11:52,12:75,13:76,14:35,15:33,16:40,17:44,18:51,19:58,20:71,21:39,22:36,23:2},
  6: {0:7,1:8,2:2,3:2,4:7,5:4,6:8,7:10,8:17,9:36,10:34,11:25,12:20,13:25,14:17,15:32,16:41,17:24,18:16,19:28,20:17,21:22,22:21,23:3},
  7: {0:0,1:6,2:4,3:1,4:2,5:2,6:3,7:20,8:32,9:42,10:54,11:66,12:35,13:42,14:67,15:36,16:63,17:53,18:29,19:38,20:41,21:39,22:3,23:0},
};

const CH_DAILY = {
  1: {"01-06":95,"01-07":99,"01-08":114,"01-09":139,"01-10":151,"01-11":166,"01-12":133,"01-13":95,"01-14":84,"01-15":98,"01-16":90,"01-17":133,"01-18":148,"01-19":142},
  2: {"01-06":50,"01-07":66,"01-08":72,"01-09":101,"01-10":115,"01-11":113,"01-12":102,"01-13":63,"01-14":62,"01-15":89,"01-16":81,"01-17":77,"01-18":91,"01-19":89},
  3: {"01-06":28,"01-07":34,"01-08":43,"01-09":52,"01-10":48,"01-11":69,"01-12":61,"01-13":43,"01-14":39,"01-15":39,"01-16":46,"01-17":43,"01-18":58,"01-19":42},
  4: {"01-06":54,"01-07":44,"01-08":60,"01-09":54,"01-10":75,"01-11":88,"01-12":82,"01-13":39,"01-14":47,"01-15":43,"01-16":59,"01-17":54,"01-18":59,"01-19":73},
  5: {"01-06":56,"01-07":53,"01-08":60,"01-09":64,"01-10":93,"01-11":101,"01-12":71,"01-13":49,"01-14":47,"01-15":49,"01-16":48,"01-17":55,"01-18":76,"01-19":67},
  6: {"01-06":36,"01-07":23,"01-08":30,"01-09":40,"01-10":42,"01-11":36,"01-12":28,"01-13":23,"01-14":20,"01-15":26,"01-16":29,"01-17":30,"01-18":34,"01-19":29},
  7: {"01-06":41,"01-07":36,"01-08":62,"01-09":41,"01-10":45,"01-11":79,"01-12":55,"01-13":52,"01-14":43,"01-15":42,"01-16":39,"01-17":52,"01-18":44,"01-19":47},
};

const WEEKLY = {
  Monday:{0:4,1:8,2:3,3:6,4:13,5:2,6:19,7:22,8:37,9:48,10:54,11:69,12:42,13:48,14:39,15:35,16:45,17:49,18:44,19:38,20:45,21:27,22:23,23:6},
  Tuesday:{0:12,1:7,2:2,3:11,4:2,5:6,6:16,7:25,8:26,9:62,10:55,11:63,12:45,13:47,14:37,15:40,16:49,17:43,18:28,19:39,20:34,21:26,22:17,23:6},
  Wednesday:{0:3,1:15,2:12,3:2,4:6,5:7,6:28,7:29,8:51,9:51,10:67,11:61,12:40,13:54,14:53,15:37,16:55,17:43,18:37,19:45,20:48,21:37,22:34,23:12},
  Thursday:{0:7,1:4,2:5,3:8,4:8,5:5,6:30,7:40,8:53,9:63,10:57,11:59,12:57,13:51,14:58,15:45,16:64,17:51,18:58,19:45,20:38,21:39,22:29,23:11},
  Friday:{0:7,1:11,2:13,3:9,4:6,5:9,6:24,7:28,8:45,9:61,10:77,11:89,12:63,13:71,14:59,15:41,16:77,17:61,18:67,19:63,20:56,21:43,22:29,23:7},
  Saturday:{0:12,1:22,2:13,3:6,4:6,5:12,6:37,7:35,8:64,9:87,10:91,11:88,12:71,13:82,14:72,15:50,16:75,17:69,18:55,19:64,20:62,21:46,22:33,23:12},
  Sunday:{0:16,1:19,2:11,3:7,4:11,5:7,6:24,7:32,8:62,9:48,10:77,11:75,12:66,13:60,14:60,15:49,16:64,17:82,18:44,19:49,20:67,21:49,22:33,23:9},
};

const HOURLY_TOTAL = {0:61,1:86,2:59,3:49,4:52,5:48,6:178,7:211,8:338,9:420,10:478,11:504,12:384,13:413,14:378,15:297,16:429,17:398,18:333,19:343,20:350,21:267,22:198,23:63};
const STRESS = { windows: 298, total: 1187, pct: 25.1 };
const TOTALS = { tx: 6337, airMin: 617.1, deadAir: 2355 };

// ============================================================
// THEME
// ============================================================
const T = {
  bg: "#06090f", surface: "#0c1219", card: "#111a25", border: "#1b2838",
  text: "#d4dce8", dim: "#7a8ba3", muted: "#4a5a70",
  orange: "#ff6b2b", orangeGlow: "rgba(255,107,43,0.15)",
  red: "#ff3b4f", green: "#2dd4a0", blue: "#3d8bfd", cyan: "#22d3ee",
  yellow: "#fbbf24", purple: "#a78bfa", pink: "#f472b6",
};
const CH_COLORS = { 1: "#ff6b2b", 2: "#3d8bfd", 3: "#fbbf24", 4: "#ff3b4f", 5: "#2dd4a0", 6: "#22d3ee", 7: "#a78bfa" };

// ============================================================
// COMPONENTS
// ============================================================
const Card = ({ children, title, sub, span }) => (
  <div style={{ background: T.card, border: `1px solid ${T.border}`, borderRadius: 10, padding: "18px 22px", gridColumn: span ? `span ${span}` : undefined }}>
    {title && <div style={{ fontSize: 11, fontWeight: 600, color: T.dim, textTransform: "uppercase", letterSpacing: "0.08em", marginBottom: sub ? 2 : 14 }}>{title}</div>}
    {sub && <div style={{ fontSize: 10, color: T.muted, marginBottom: 14 }}>{sub}</div>}
    {children}
  </div>
);

const Metric = ({ value, unit, label, color = T.orange, size = 38 }) => (
  <div style={{ textAlign: "center" }}>
    <div style={{ fontSize: size, fontWeight: 800, color, fontFamily: "'Space Mono', monospace", lineHeight: 1, letterSpacing: "-0.03em" }}>
      {value}{unit && <span style={{ fontSize: size * 0.4, color: T.muted, fontWeight: 400 }}>{unit}</span>}
    </div>
    {label && <div style={{ fontSize: 10, color: T.muted, marginTop: 6, textTransform: "uppercase", letterSpacing: "0.08em" }}>{label}</div>}
  </div>
);

const ChannelBadge = ({ ch }) => (
  <span style={{ display: "inline-flex", alignItems: "center", gap: 5, padding: "2px 8px", borderRadius: 4, background: `${CH_COLORS[ch]}15`, border: `1px solid ${CH_COLORS[ch]}30`, fontSize: 11, color: CH_COLORS[ch], fontWeight: 600, fontFamily: "'Space Mono', monospace" }}>
    CH{ch}
  </span>
);

const Tip = ({ active, payload, label }) => {
  if (!active || !payload?.length) return null;
  return (
    <div style={{ background: T.card, border: `1px solid ${T.border}`, borderRadius: 6, padding: "6px 10px", fontSize: 11 }}>
      <div style={{ color: T.dim, marginBottom: 3 }}>{label}</div>
      {payload.map((p, i) => (
        <div key={i} style={{ color: p.color || T.text, fontFamily: "'Space Mono', monospace", fontSize: 11 }}>
          {p.name}: {typeof p.value === "number" ? Math.round(p.value * 10) / 10 : p.value}
        </div>
      ))}
    </div>
  );
};

const Bar2 = ({ label, value, max, color, sub }) => {
  const pct = max > 0 ? (value / max) * 100 : 0;
  return (
    <div style={{ marginBottom: 10 }}>
      <div style={{ display: "flex", justifyContent: "space-between", marginBottom: 3 }}>
        <span style={{ fontSize: 12, color: T.text }}>{label}</span>
        <span style={{ fontSize: 12, color, fontFamily: "'Space Mono', monospace", fontWeight: 600 }}>{value}{sub}</span>
      </div>
      <div style={{ height: 6, background: T.bg, borderRadius: 3, overflow: "hidden" }}>
        <div style={{ width: `${Math.min(pct, 100)}%`, height: "100%", background: `linear-gradient(90deg, ${color}, ${color}aa)`, borderRadius: 3, transition: "width 0.6s cubic-bezier(0.16,1,0.3,1)" }} />
      </div>
    </div>
  );
};

// ============================================================
// TABS
// ============================================================
const TABS = [
  { id: "live", label: "Live Overview", icon: "◉" },
  { id: "channels", label: "Channel Detail", icon: "▥" },
  { id: "stress", label: "Stress Map", icon: "△" },
  { id: "coverage", label: "Coverage Gaps", icon: "◌" },
  { id: "signal", label: "Signal & Duration", icon: "∿" },
];

// ============================================================
// LIVE OVERVIEW
// ============================================================
const LiveOverview = () => {
  const chList = Object.entries(CH).sort((a, b) => b[1].tx - a[1].tx);
  const maxTx = chList[0][1].tx;

  // Simulated "live" pulse
  const [pulse, setPulse] = useState(true);
  useEffect(() => { const t = setInterval(() => setPulse(p => !p), 2000); return () => clearInterval(t); }, []);

  // All-channel hourly for sparkline
  const hourlyData = Object.entries(HOURLY_TOTAL).map(([h, v]) => ({ h: `${h.padStart(2, "0")}:00`, v }));

  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 14 }}>
      {/* Top metrics */}
      <Card title="Total Transmissions" sub="14-day capture window">
        <Metric value={TOTALS.tx.toLocaleString()} label="PTT Events Recorded" />
      </Card>
      <Card title="Total Air Time" sub="Cumulative channel usage">
        <Metric value={TOTALS.airMin.toLocaleString()} unit=" min" label="Radio Active" color={T.blue} />
      </Card>
      <Card title="Stress Windows" sub="15-min windows with 5+ channels active">
        <Metric value={`${STRESS.pct}%`} unit="" label={`${STRESS.windows} of ${STRESS.total} windows`} color={T.red} />
      </Card>
      <Card title="Coverage Gaps" sub="Dead air events (>15 min silence)">
        <Metric value={TOTALS.deadAir.toLocaleString()} label="Across All Channels" color={T.yellow} />
      </Card>

      {/* Hourly volume curve */}
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
            <Area type="monotone" dataKey="v" name="Transmissions" stroke={T.orange} fill="url(#gAll)" strokeWidth={2.5} dot={false} />
          </AreaChart>
        </ResponsiveContainer>
      </Card>

      {/* Channel rankings */}
      <Card title="Channel Activity Ranking" sub="Daily average transmissions" span={2}>
        {chList.map(([ch, d]) => (
          <div key={ch} style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 10 }}>
            <ChannelBadge ch={ch} />
            <div style={{ flex: 1 }}>
              <Bar2 label={d.short} value={d.daily} max={maxTx / 14} color={CH_COLORS[ch]} sub="/day" />
            </div>
          </div>
        ))}
      </Card>

      {/* Channel composition pie */}
      <Card title="Channel Share" sub="% of total transmissions" span={2}>
        <ResponsiveContainer width="100%" height={260}>
          <PieChart>
            <Pie
              data={chList.map(([ch, d]) => ({ name: `CH${ch} ${d.short}`, value: d.tx, fill: CH_COLORS[ch] }))}
              dataKey="value" nameKey="name" cx="50%" cy="50%" outerRadius={95} innerRadius={50} paddingAngle={2} strokeWidth={0}
            >
              {chList.map(([ch]) => <Cell key={ch} fill={CH_COLORS[ch]} />)}
            </Pie>
            <Tooltip content={<Tip />} />
            <Legend wrapperStyle={{ fontSize: 10, color: T.dim }} />
          </PieChart>
        </ResponsiveContainer>
      </Card>

      {/* Channel status cards */}
      <div style={{ gridColumn: "span 4", display: "grid", gridTemplateColumns: "repeat(7, 1fr)", gap: 8 }}>
        {Object.entries(CH).map(([ch, d]) => (
          <div key={ch} style={{
            background: T.surface, border: `1px solid ${CH_COLORS[ch]}25`, borderRadius: 8, padding: "12px 10px",
            borderTop: `2px solid ${CH_COLORS[ch]}`,
          }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 8 }}>
              <span style={{ fontSize: 10, fontWeight: 700, color: CH_COLORS[ch], fontFamily: "'Space Mono', monospace" }}>CH{ch}</span>
              <span style={{ width: 6, height: 6, borderRadius: "50%", background: pulse ? T.green : `${T.green}40`, transition: "background 0.5s" }} />
            </div>
            <div style={{ fontSize: 10, color: T.dim, marginBottom: 4 }}>{d.short}</div>
            <div style={{ fontSize: 18, fontWeight: 800, color: T.text, fontFamily: "'Space Mono', monospace", lineHeight: 1 }}>{d.daily}</div>
            <div style={{ fontSize: 9, color: T.muted }}>tx/day</div>
            <div style={{ marginTop: 8, fontSize: 9, color: T.muted, display: "flex", justifyContent: "space-between" }}>
              <span>Cadence</span>
              <span style={{ color: T.dim, fontFamily: "'Space Mono', monospace" }}>{Math.round(d.medCadence / 60)}m</span>
            </div>
            <div style={{ fontSize: 9, color: T.muted, display: "flex", justifyContent: "space-between" }}>
              <span>Dead Air</span>
              <span style={{ color: d.deadAir > 350 ? T.red : T.dim, fontFamily: "'Space Mono', monospace" }}>{d.deadAir}</span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};

// ============================================================
// CHANNEL DETAIL
// ============================================================
const ChannelDetail = () => {
  const [sel, setSel] = useState(1);
  const d = CH[sel];
  const color = CH_COLORS[sel];

  const hourly = Object.entries(CH_HOURLY[sel]).map(([h, v]) => ({ h: `${h.padStart(2, "0")}`, v }));
  const daily = Object.entries(CH_DAILY[sel]).map(([d, v]) => ({ d, v }));

  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: 14 }}>
      {/* Channel selector */}
      <div style={{ gridColumn: "span 4", display: "flex", gap: 6, flexWrap: "wrap" }}>
        {Object.entries(CH).map(([ch, cd]) => (
          <button key={ch} onClick={() => setSel(Number(ch))} style={{
            padding: "8px 14px", borderRadius: 6, border: `1px solid ${Number(ch) === sel ? CH_COLORS[ch] : T.border}`,
            background: Number(ch) === sel ? `${CH_COLORS[ch]}15` : T.surface, cursor: "pointer",
            color: Number(ch) === sel ? CH_COLORS[ch] : T.dim, fontSize: 12, fontWeight: 600,
            fontFamily: "'Space Mono', monospace",
          }}>
            CH{ch} {cd.short}
          </button>
        ))}
      </div>

      {/* Stats */}
      <Card title={`CH${sel} · ${d.name}`} sub="Channel Performance Summary">
        <Metric value={d.daily} unit="/day" label="Avg Transmissions" color={color} size={32} />
        <div style={{ marginTop: 16 }}>
          <Bar2 label="Avg Duration" value={d.avgDur} max={10} color={color} sub="s" />
          <Bar2 label="Med Cadence" value={Math.round(d.medCadence / 60)} max={25} color={color} sub=" min" />
          <Bar2 label="Utilization" value={d.util} max={1.5} color={color} sub="%" />
          <Bar2 label="Dead Air" value={d.deadAir} max={450} color={d.deadAir > 350 ? T.red : color} sub=" events" />
        </div>
      </Card>

      {/* Hourly curve */}
      <Card title="Hourly Distribution" sub="Transmissions by hour of day" span={2}>
        <ResponsiveContainer width="100%" height={220}>
          <BarChart data={hourly}>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="h" tick={{ fill: T.muted, fontSize: 9 }} interval={1} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} />
            <Tooltip content={<Tip />} />
            <Bar dataKey="v" name="Transmissions" radius={[3,3,0,0]}>
              {hourly.map((e, i) => <Cell key={i} fill={e.v > (d.tx / 24) * 1.3 ? color : `${color}60`} />)}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Card>

      {/* TX composition */}
      <Card title="Transmission Profile" sub="Duration breakdown">
        <div style={{ padding: "10px 0" }}>
          {[
            { label: "Short (<3s)", pct: d.shortPct, desc: "Quick acks, confirmations", color: T.green },
            { label: "Medium (3-10s)", pct: d.medPct, desc: "Standard requests, updates", color: T.blue },
            { label: "Long (>10s)", pct: d.longPct, desc: "Complex requests, confusion", color: T.red },
          ].map(t => (
            <div key={t.label} style={{ marginBottom: 14 }}>
              <div style={{ display: "flex", justifyContent: "space-between", fontSize: 11, marginBottom: 4 }}>
                <span style={{ color: T.text }}>{t.label}</span>
                <span style={{ color: t.color, fontFamily: "'Space Mono', monospace", fontWeight: 700 }}>{t.pct}%</span>
              </div>
              <div style={{ height: 8, background: T.bg, borderRadius: 4, overflow: "hidden" }}>
                <div style={{ width: `${t.pct}%`, height: "100%", background: t.color, borderRadius: 4 }} />
              </div>
              <div style={{ fontSize: 9, color: T.muted, marginTop: 2 }}>{t.desc}</div>
            </div>
          ))}
        </div>
      </Card>

      {/* Daily trend */}
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
            <Area type="monotone" dataKey="v" name="Transmissions" stroke={color} fill={`url(#gCh${sel})`} strokeWidth={2} dot={{ fill: color, r: 2.5 }} />
          </AreaChart>
        </ResponsiveContainer>
      </Card>
    </div>
  );
};

// ============================================================
// STRESS MAP
// ============================================================
const StressMap = () => {
  const days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
  const dayKeys = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"];
  const hours = Array.from({ length: 24 }, (_, i) => i);

  let maxV = 0;
  dayKeys.forEach(d => hours.forEach(h => { const v = WEEKLY[d]?.[h] || 0; if (v > maxV) maxV = v; }));

  // Stacked channel hourly
  const stackedData = hours.map(h => {
    const row = { h: `${String(h).padStart(2, "0")}:00` };
    Object.entries(CH_HOURLY).forEach(([ch, hData]) => {
      row[`ch${ch}`] = hData[h] || 0;
    });
    return row;
  });

  return (
    <div style={{ display: "grid", gridTemplateColumns: "1fr", gap: 14 }}>
      <Card title="Weekly Stress Heatmap" sub="All-channel volume by day of week and hour - identifies operational pressure points">
        <div style={{ overflowX: "auto", paddingBottom: 8 }}>
          <div style={{ display: "grid", gridTemplateColumns: `50px repeat(24, 1fr)`, gap: 2, minWidth: 700 }}>
            <div />
            {hours.map(h => (
              <div key={h} style={{ textAlign: "center", color: T.muted, fontSize: 9, paddingBottom: 4, fontFamily: "'Space Mono', monospace" }}>{h}</div>
            ))}
            {dayKeys.map((day, di) => (
              <>
                <div key={day} style={{ color: T.dim, fontSize: 11, display: "flex", alignItems: "center", fontWeight: 600 }}>{days[di]}</div>
                {hours.map(h => {
                  const val = WEEKLY[day]?.[h] || 0;
                  const intensity = maxV > 0 ? val / maxV : 0;
                  const isHot = intensity > 0.7;
                  return (
                    <div key={`${day}-${h}`} style={{
                      background: val === 0 ? T.bg : `rgba(255,107,43,${intensity * 0.85})`,
                      borderRadius: 2, height: 28, display: "flex", alignItems: "center", justifyContent: "center",
                      color: isHot ? "#fff" : intensity > 0.3 ? T.text : T.muted,
                      fontSize: 9, fontFamily: "'Space Mono', monospace", fontWeight: isHot ? 700 : 400,
                      border: isHot ? `1px solid ${T.orange}60` : "1px solid transparent",
                    }}>
                      {val > 0 ? val : ""}
                    </div>
                  );
                })}
              </>
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
            {Object.keys(CH).map(ch => (
              <Bar key={ch} dataKey={`ch${ch}`} name={CH[ch].short} stackId="a" fill={CH_COLORS[ch]} />
            ))}
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 14 }}>
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
};

// ============================================================
// COVERAGE GAPS
// ============================================================
const CoverageGaps = () => {
  const gapData = Object.entries(CH)
    .map(([ch, d]) => ({ ch: Number(ch), name: `CH${ch} ${d.short}`, deadAir: d.deadAir, cadence: Math.round(d.cadence / 60), medCadence: Math.round(d.medCadence / 60), color: CH_COLORS[ch] }))
    .sort((a, b) => b.deadAir - a.deadAir);

  const maxDead = gapData[0].deadAir;

  // Calculate overnight gaps
  const overnightChannels = Object.entries(CH_HOURLY)
    .filter(([ch]) => Number(ch) !== 4) // exclude security
    .map(([ch, h]) => {
      const nightTx = (h[0] || 0) + (h[1] || 0) + (h[2] || 0) + (h[3] || 0) + (h[4] || 0) + (h[5] || 0);
      return { ch: Number(ch), name: CH[ch].short, nightTx, color: CH_COLORS[ch] };
    })
    .sort((a, b) => a.nightTx - b.nightTx);

  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 14 }}>
      <Card title="Dead Air Events by Channel" sub="Gaps >15 min during active hours — indicates staffing holes">
        {gapData.map(d => (
          <div key={d.ch} style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 8 }}>
            <ChannelBadge ch={d.ch} />
            <div style={{ flex: 1 }}>
              <Bar2 label={d.name.split(" ").slice(1).join(" ")} value={d.deadAir} max={maxDead} color={d.deadAir > 350 ? T.red : d.color} sub="" />
            </div>
          </div>
        ))}
      </Card>

      <Card title="Response Cadence" sub="Median time between consecutive transmissions on same channel">
        <ResponsiveContainer width="100%" height={260}>
          <BarChart data={gapData.sort((a, b) => b.medCadence - a.medCadence)}>
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} />
            <XAxis dataKey="name" tick={{ fill: T.muted, fontSize: 9 }} />
            <YAxis tick={{ fill: T.muted, fontSize: 9 }} unit="m" />
            <Tooltip content={<Tip />} />
            <Bar dataKey="medCadence" name="Median Cadence (min)" radius={[4,4,0,0]}>
              {gapData.map((d, i) => <Cell key={i} fill={d.medCadence > 15 ? T.yellow : d.color} />)}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <Card title="Overnight Activity" sub="Transmissions between 12am-6am (excluding Security)" span={2}>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 10 }}>
          {overnightChannels.map(d => (
            <div key={d.ch} style={{
              padding: "14px 16px", background: T.surface, borderRadius: 8,
              border: `1px solid ${d.nightTx < 15 ? `${T.red}40` : T.border}`,
              borderLeft: `3px solid ${d.nightTx < 15 ? T.red : d.color}`,
            }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                <div>
                  <div style={{ fontSize: 10, color: T.dim }}><ChannelBadge ch={d.ch} /> {d.name}</div>
                  <div style={{ fontSize: 10, color: T.muted, marginTop: 4 }}>
                    {d.nightTx < 15 ? "⚠ Near-silent overnight" : d.nightTx < 30 ? "Low overnight activity" : "Active overnight"}
                  </div>
                </div>
                <div style={{ fontSize: 24, fontWeight: 800, color: d.nightTx < 15 ? T.red : T.text, fontFamily: "'Space Mono', monospace" }}>{d.nightTx}</div>
              </div>
            </div>
          ))}
        </div>
        <div style={{ marginTop: 12, padding: "10px 14px", background: `${T.red}08`, border: `1px solid ${T.red}20`, borderRadius: 6, fontSize: 11, color: T.dim }}>
          Channels with near-zero overnight activity represent coverage blind spots. If a guest calls front desk at 2am about a plumbing emergency, is engineering's radio monitored?
        </div>
      </Card>
    </div>
  );
};

// ============================================================
// SIGNAL & DURATION
// ============================================================
const SignalDuration = () => {
  const durData = Object.entries(CH)
    .map(([ch, d]) => ({ ch: Number(ch), name: `CH${ch} ${d.short}`, avg: d.avgDur, med: d.medDur, max: d.maxDur, color: CH_COLORS[ch] }))
    .sort((a, b) => b.avg - a.avg);

  const sigData = Object.entries(CH).map(([ch, d]) => ({
    name: `CH${ch}`, signal: Math.abs(d.signal), weakPct: d.weakPct, color: CH_COLORS[ch],
  }));

  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 14 }}>
      <Card title="Avg Transmission Duration" sub="Longer = more complex requests or operational confusion">
        <ResponsiveContainer width="100%" height={260}>
          <BarChart data={durData} layout="vertical">
            <CartesianGrid strokeDasharray="3 3" stroke={T.border} horizontal={false} />
            <XAxis type="number" tick={{ fill: T.muted, fontSize: 9 }} unit="s" />
            <YAxis type="category" dataKey="name" tick={{ fill: T.dim, fontSize: 10 }} width={120} />
            <Tooltip content={<Tip />} />
            <Bar dataKey="avg" name="Avg Duration (s)" radius={[0,4,4,0]}>
              {durData.map((d, i) => <Cell key={i} fill={d.avg > 7 ? T.yellow : d.color} />)}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </Card>

      <Card title="Duration Profile Comparison" sub="What type of transmissions dominate each channel">
        <div style={{ display: "grid", gridTemplateColumns: "1fr", gap: 6, marginTop: 4 }}>
          {Object.entries(CH).map(([ch, d]) => (
            <div key={ch} style={{ display: "flex", alignItems: "center", gap: 8, fontSize: 11 }}>
              <span style={{ width: 40, color: CH_COLORS[ch], fontFamily: "'Space Mono', monospace", fontWeight: 700, fontSize: 10, flexShrink: 0 }}>CH{ch}</span>
              <div style={{ flex: 1, display: "flex", height: 16, borderRadius: 3, overflow: "hidden" }}>
                <div style={{ width: `${d.shortPct}%`, background: T.green, transition: "width 0.5s" }} title={`Short: ${d.shortPct}%`} />
                <div style={{ width: `${d.medPct}%`, background: T.blue, transition: "width 0.5s" }} title={`Medium: ${d.medPct}%`} />
                <div style={{ width: `${d.longPct}%`, background: T.red, transition: "width 0.5s" }} title={`Long: ${d.longPct}%`} />
              </div>
              <span style={{ width: 35, color: d.longPct > 20 ? T.red : T.muted, fontFamily: "'Space Mono', monospace", fontSize: 10, textAlign: "right" }}>{d.longPct}%</span>
            </div>
          ))}
        </div>
        <div style={{ display: "flex", gap: 16, marginTop: 12, fontSize: 10, color: T.muted }}>
          <span><span style={{ display: "inline-block", width: 8, height: 8, borderRadius: 2, background: T.green, marginRight: 4 }} />Short (&lt;3s)</span>
          <span><span style={{ display: "inline-block", width: 8, height: 8, borderRadius: 2, background: T.blue, marginRight: 4 }} />Medium (3-10s)</span>
          <span><span style={{ display: "inline-block", width: 8, height: 8, borderRadius: 2, background: T.red, marginRight: 4 }} />Long (&gt;10s)</span>
        </div>
        <div style={{ marginTop: 12, padding: "10px 14px", background: `${T.yellow}08`, border: `1px solid ${T.yellow}20`, borderRadius: 6, fontSize: 11, color: T.dim }}>
          Engineering (CH3) has 36.7% long transmissions — highest by far. Long transmissions often indicate complex issues being described verbally that would benefit from structured dispatch.
        </div>
      </Card>

      <Card title="Signal Quality" sub="Average signal strength and weak signal events" span={2}>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(7, 1fr)", gap: 8 }}>
          {Object.entries(CH).map(([ch, d]) => (
            <div key={ch} style={{ textAlign: "center", padding: "12px 8px", background: T.surface, borderRadius: 8, border: `1px solid ${T.border}` }}>
              <div style={{ fontSize: 10, color: CH_COLORS[ch], fontWeight: 700, fontFamily: "'Space Mono', monospace", marginBottom: 6 }}>CH{ch}</div>
              <div style={{ fontSize: 20, fontWeight: 800, color: T.text, fontFamily: "'Space Mono', monospace" }}>{Math.round(d.signal)}</div>
              <div style={{ fontSize: 9, color: T.muted }}>dBm avg</div>
              <div style={{ fontSize: 10, color: d.weakPct > 4 ? T.yellow : T.dim, fontFamily: "'Space Mono', monospace", marginTop: 4 }}>{d.weakPct}%</div>
              <div style={{ fontSize: 9, color: T.muted }}>weak signal</div>
            </div>
          ))}
        </div>
      </Card>
    </div>
  );
};

// ============================================================
// MAIN
// ============================================================
const TAB_MAP = { live: LiveOverview, channels: ChannelDetail, stress: StressMap, coverage: CoverageGaps, signal: SignalDuration };

export default function Dashboard() {
  const [tab, setTab] = useState("live");
  const View = TAB_MAP[tab];

  return (
    <div style={{ background: T.bg, minHeight: "100vh", color: T.text, fontFamily: "'Inter', -apple-system, sans-serif" }}>
      <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&family=Space+Mono:wght@400;700&display=swap" rel="stylesheet" />

      {/* Header */}
      <div style={{
        padding: "16px 28px", display: "flex", alignItems: "center", justifyContent: "space-between",
        borderBottom: `1px solid ${T.border}`, background: `linear-gradient(180deg, ${T.card} 0%, ${T.bg} 100%)`,
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 14 }}>
          <div style={{
            width: 32, height: 32, borderRadius: 8, background: `linear-gradient(135deg, ${T.orange}, ${T.red})`,
            display: "flex", alignItems: "center", justifyContent: "center", fontSize: 14, fontWeight: 800, color: "#fff",
          }}>M</div>
          <div>
            <div style={{ fontSize: 16, fontWeight: 800, letterSpacing: "-0.02em" }}>
              Memo <span style={{ color: T.orange }}>Radio Intelligence</span>
            </div>
            <div style={{ fontSize: 10, color: T.muted }}>Fairmont Hotel · Channel Monitor · Jan 6–19, 2025</div>
          </div>
        </div>
        <div style={{ display: "flex", gap: 20, fontSize: 11, color: T.dim }}>
          <span><span style={{ fontFamily: "'Space Mono', monospace", color: T.text, fontWeight: 700 }}>7</span> channels</span>
          <span><span style={{ fontFamily: "'Space Mono', monospace", color: T.text, fontWeight: 700 }}>{TOTALS.tx.toLocaleString()}</span> PTT events</span>
          <span><span style={{ fontFamily: "'Space Mono', monospace", color: T.text, fontWeight: 700 }}>14</span> days</span>
          <span style={{ display: "flex", alignItems: "center", gap: 4 }}>
            <span style={{ width: 6, height: 6, borderRadius: "50%", background: T.green, boxShadow: `0 0 6px ${T.green}` }} />
            <span style={{ color: T.green, fontWeight: 600 }}>MONITORING</span>
          </span>
        </div>
      </div>

      {/* Tabs */}
      <div style={{ display: "flex", gap: 0, borderBottom: `1px solid ${T.border}`, padding: "0 28px", overflowX: "auto" }}>
        {TABS.map(t => (
          <button key={t.id} onClick={() => setTab(t.id)} style={{
            padding: "11px 18px", fontSize: 12, fontWeight: tab === t.id ? 700 : 400,
            color: tab === t.id ? T.orange : T.muted, background: "none", border: "none", cursor: "pointer",
            borderBottom: tab === t.id ? `2px solid ${T.orange}` : "2px solid transparent",
            fontFamily: "'Inter', sans-serif", display: "flex", alignItems: "center", gap: 6, whiteSpace: "nowrap",
          }}>
            <span style={{ fontSize: 10 }}>{t.icon}</span> {t.label}
          </button>
        ))}
      </div>

      {/* Content */}
      <div style={{ padding: "20px 28px", maxWidth: 1200 }}>
        <View />
      </div>
    </div>
  );
}
