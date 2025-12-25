import "./App.css";
import { useMemo } from "react";
import {
  Area,
  AreaChart,
  CartesianGrid,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import { useFeed } from "./useFeed";
import { KpiCard } from "./components/KpiCard";

const fmtTime = (ts?: number) =>
  ts ? new Date(ts).toLocaleTimeString([], { hour12: false }) : "—";

export default function App() {
  const { status, ticks, config, feedUrl } = useFeed();
  const latest = ticks[ticks.length - 1];

  const netSeries = useMemo(
    () =>
      ticks.map((t) => ({
        ts: t.ts,
        net: t.net_cost_usd ?? 0,
        fee: t.fee_usd ?? 0,
        impact: t.ac_impact_usd ?? 0,
      })),
    [ticks]
  );

  const slipSeries = useMemo(
    () =>
      ticks.map((t) => ({
        ts: t.ts,
        vwap: t.vwap_slip_pct ?? 0,
        model: t.model_slip_pct ?? 0,
      })),
    [ticks]
  );

  const edge =
    (latest?.vwap_slip_pct ?? 0) - (latest?.model_slip_pct ?? 0);

  const summary = useMemo(() => {
    const recent = ticks.slice(-20);
    if (recent.length === 0) {
      return {
        avgNet: 0,
        maxSlip: 0,
        minSpread: 0,
        avgLatency: 0,
        bestDepth: 0,
      };
    }
    const avgNet =
      recent.reduce((s, t) => s + (t.net_cost_usd ?? 0), 0) / recent.length;
    const maxSlip = Math.max(...recent.map((t) => t.vwap_slip_pct ?? 0));
    const minSpread = Math.min(
      ...recent.map((t) => (t.spread ?? Number.POSITIVE_INFINITY))
    );
    const avgLatency =
      recent.reduce((s, t) => s + (t.tick_latency_ms ?? 0), 0) /
      recent.filter((t) => t.tick_latency_ms !== undefined).length || 0;
    const bestDepth = Math.max(...recent.map((t) => t.depth_top5 ?? 0));
    return { avgNet, maxSlip, minSpread, avgLatency, bestDepth };
  }, [ticks]);

  return (
    <div className="page">
      <header className="header hero">
        <div>
          <div className="eyebrow">OKX Live • Market Buy Simulation</div>
          <div className="title">QuantSim Execution Desk</div>
          <div className="subtitle">
            Real-time VWAP slippage, fee, and Almgren–Chriss impact with maker/taker tilt. Built for explaining execution quality to HFT leads.
          </div>
          <div className="meta">
            <span>Feed: {feedUrl}</span>
            <span>Symbol: {latest?.symbol ?? config?.symbol ?? "—"}</span>
            <span>Params: ${latest?.notional ?? config?.notional ?? "—"} · {config?.sigma ? `σ ${config.sigma}` : "σ —"} · λ {config?.lambda ?? "—"}</span>
          </div>
        </div>
        <div className="meta right">
          <span className={`pill ${status}`}>{status.toUpperCase()}</span>
          <span>Last tick: {fmtTime(latest?.ts)}</span>
          <span>
            Latency:{" "}
            {latest?.tick_latency_ms
              ? `${latest.tick_latency_ms.toFixed(0)} ms`
              : "—"}
          </span>
          <span>Drops: {latest?.drops_in_window ?? 0}</span>
        </div>
      </header>

      <div className="kpi-grid">
        <KpiCard
          label="Net Cost"
          value={`$${(latest?.net_cost_usd ?? 0).toFixed(2)}`}
          sub={`Fee $${(latest?.fee_usd ?? 0).toFixed(2)} · Impact $${(latest?.ac_impact_usd ?? 0).toFixed(2)}`}
          tone="bad"
        />
        <KpiCard
          label="VWAP Slip"
          value={`${(latest?.vwap_slip_pct ?? 0).toFixed(4)}%`}
          sub="Actual"
          tone="bad"
        />
        <KpiCard
          label="Model Slip"
          value={`${(latest?.model_slip_pct ?? 0).toFixed(4)}%`}
          sub="Predicted"
          tone="neutral"
        />
        <KpiCard
          label="Edge vs Model"
          value={`${edge.toFixed(4)}%`}
          sub="(Actual - Predicted)"
          tone={edge > 0 ? "bad" : "good"}
        />
        <KpiCard
          label="Taker Prob."
          value={`${(latest?.taker_prob_pct ?? 0).toFixed(2)}%`}
          sub="Logistic model"
          tone="bad"
        />
        <KpiCard
          label="Spread"
          value={`${(latest?.spread ?? 0).toFixed(2)} USD`}
          sub="Top of book"
          tone="neutral"
        />
        <KpiCard
          label="Depth Top 5"
          value={`${(latest?.depth_top5 ?? 0).toFixed(2)} units`}
          sub="Asks (Top 5)"
          tone="good"
        />
        <KpiCard
          label="Imbalance"
          value={`${(((latest?.imbalance ?? 0) * 100) || 0).toFixed(1)}%`}
          sub="(bid-ask)/sum"
          tone="neutral"
        />
      </div>

      <div className="chart-row">
        <div className="card">
          <div className="section-title">Net Cost Components</div>
          <ResponsiveContainer width="100%" height={260}>
            <AreaChart data={netSeries}>
              <defs>
                <linearGradient id="netGrad" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="0%" stopColor="#22c55e" stopOpacity={0.5} />
                  <stop offset="100%" stopColor="#22c55e" stopOpacity={0.05} />
                </linearGradient>
              </defs>
              <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
              <XAxis
                dataKey="ts"
                tickFormatter={(t) => fmtTime(t)}
                stroke="#6b7280"
                fontSize={12}
              />
              <YAxis stroke="#6b7280" fontSize={12} />
              <Tooltip
                formatter={(value: number | string | undefined) =>
                  `$${Number(value ?? 0).toFixed(2)}`
                }
                labelFormatter={(v) => fmtTime(Number(v))}
              />
              <Area
                type="monotone"
                dataKey="net"
                stroke="#22c55e"
                fill="url(#netGrad)"
                strokeWidth={2}
              />
              <Line type="monotone" dataKey="fee" stroke="#f59e0b" dot={false} />
              <Line type="monotone" dataKey="impact" stroke="#38bdf8" dot={false} />
            </AreaChart>
          </ResponsiveContainer>
        </div>

        <div className="card">
          <div className="section-title">Slippage: Actual vs Model</div>
          <ResponsiveContainer width="100%" height={260}>
            <LineChart data={slipSeries}>
              <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
              <XAxis
                dataKey="ts"
                tickFormatter={(t) => fmtTime(t)}
                stroke="#6b7280"
                fontSize={12}
              />
              <YAxis stroke="#6b7280" fontSize={12} />
              <Tooltip
                formatter={(value: number | string | undefined) =>
                  `${Number(value ?? 0).toFixed(4)}%`
                }
                labelFormatter={(v) => fmtTime(Number(v))}
              />
              <Line type="monotone" dataKey="vwap" stroke="#f43f5e" dot={false} />
              <Line type="monotone" dataKey="model" stroke="#22d3ee" dot={false} />
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>

      <div className="micro-row">
        <div className="card">
          <div className="section-title">Microstructure Snapshot</div>
          <div className="mini-grid">
            <div className="chip">
              Best Bid: {latest?.best_bid ? latest.best_bid.toFixed(2) : "—"}
            </div>
            <div className="chip">
              Best Ask: {latest?.best_ask ? latest.best_ask.toFixed(2) : "—"}
            </div>
            <div className="chip">
              Mid: {latest?.mid ? latest.mid.toFixed(2) : "—"}
            </div>
            <div className="chip">
              Depth (bids top 5):{" "}
              {latest?.depth_top5_bids ? latest.depth_top5_bids.toFixed(2) : "—"}
            </div>
          </div>
          <div className="divider" />
          <div className="mini-grid">
            <div className="chip">
              Tick Latency:{" "}
              {latest?.tick_latency_ms
                ? `${latest.tick_latency_ms.toFixed(0)} ms`
                : "—"}
            </div>
            <div className="chip">Drops (window): {latest?.drops_in_window ?? 0}</div>
            <div className="chip">Interval: {config?.interval_sec ?? "—"}s</div>
            <div className="chip">Sigma: {(config?.sigma ?? 0).toFixed(4)}</div>
          </div>
        </div>

        <div className="card">
          <div className="section-title">Execution If Sent Now</div>
          <div className="mini-grid">
            <div className="chip">
              Notional: ${latest?.notional?.toFixed(0) ?? config?.notional ?? "—"}
            </div>
            <div className="chip">
              Fee: ${(latest?.fee_usd ?? 0).toFixed(2)}
            </div>
            <div className="chip">
              Impact: ${(latest?.ac_impact_usd ?? 0).toFixed(2)}
            </div>
            <div className="chip">
              Model Slip: ${(latest?.model_slip_usd ?? 0).toFixed(4)}
            </div>
          </div>
          <div className="divider" />
          <div style={{ color: "#cbd5e1", fontSize: 14 }}>
            Estimated total cost: ${ (latest?.net_cost_usd ?? 0).toFixed(2) }{" "}
            {latest?.status === "insufficient_depth" ? "(insufficient depth)" : ""}
          </div>
          <div style={{ color: "#94a3b8", fontSize: 12, marginTop: 6 }}>
            Maker/Taker tilt: {(latest?.taker_prob_pct ?? 0).toFixed(2)}% taker
          </div>
        </div>

        <div className="card">
          <div className="section-title">Recent Events</div>
          <div className="log">
            {ticks.slice(-6).map((t) => (
              <div key={t.ts} className="log-line">
                <span>{fmtTime(t.ts)}</span>
                <span>
                  Net ${ (t.net_cost_usd ?? 0).toFixed(2)} · Slip {(t.vwap_slip_pct ?? 0).toFixed(4)}%
                </span>
              </div>
            ))}
          </div>
        </div>
        <div className="card narrative">
          <div className="section-title">Execution Insights (last 20 runs)</div>
          <ul className="insight-list">
            <li>
              Avg net cost: ${summary.avgNet.toFixed(2)} | Max slip:{" "}
              {summary.maxSlip.toFixed(4)}%
            </li>
            <li>
              Tightest spread seen:{" "}
              {summary.minSpread === Number.POSITIVE_INFINITY
                ? "—"
                : `${summary.minSpread.toFixed(2)} USD`}
              ; Best top-5 depth: {summary.bestDepth.toFixed(2)} units
            </li>
            <li>
              Avg tick latency:{" "}
              {summary.avgLatency ? `${summary.avgLatency.toFixed(0)} ms` : "—"}
            </li>
            <li>
              Maker/Taker tilt (latest):{" "}
              {(latest?.taker_prob_pct ?? 0).toFixed(2)}% taker
            </li>
          </ul>
        </div>
      </div>

      <div className="explainer">
        <div className="card narrative">
          <div className="section-title">What this dashboard is doing</div>
          <ul>
            <li>Live OKX L2 feed → thread-safe order book → simulate a market buy every {config?.interval_sec ?? 5}s for the chosen notional.</li>
            <li>Actual cost = VWAP from walking the asks + taker fee. Modeled cost = regression on spread and depth. Impact = Almgren–Chriss term.</li>
            <li>Maker/taker probability uses a logistic model on spread/depth so we can see fee exposure.</li>
            <li>Latency/drops show data quality. Imbalance shows whether bids or asks dominate the top of book.</li>
            <li>Everything in the KPIs and charts comes straight from each simulated fill so it’s easy to defend the numbers.</li>
          </ul>
        </div>
        <div className="card narrative">
          <div className="section-title">How to read it</div>
          <ul>
            <li>Net Cost Components: separates fee, predicted impact, and modeled slippage so we know what drives cost drift.</li>
            <li>Slippage Actual vs Model: orange edge means the model underestimates current conditions; blue means we’re doing better than predicted.</li>
            <li>Microstructure: spread, bid/ask depth, imbalance, and latency explain why fills improved or degraded.</li>
            <li>Execution If Sent Now: current all-in cost for this notional—use it when discussing “what if we crossed right now”.</li>
          </ul>
        </div>
      </div>
    </div>
  );
}
