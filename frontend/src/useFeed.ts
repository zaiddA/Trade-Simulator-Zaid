import { useEffect, useRef, useState } from "react";

export type Tick = {
  ts: number;
  symbol: string;
  notional: number;
  taker_bps: number;
  spread: number;
  depth_top5?: number;
  depth_top5_bids?: number;
  best_bid?: number;
  best_ask?: number;
  mid?: number;
  vwap_slip_pct?: number;
  model_slip_pct?: number;
  model_slip_usd?: number;
  fee_usd?: number;
  ac_impact_usd?: number;
  net_cost_usd?: number;
  taker_prob_pct?: number;
  tick_latency_ms?: number;
  drops_in_window?: number;
  imbalance?: number;
  status?: string;
};

export type FeedStatus = "connecting" | "live" | "degraded" | "down";

const defaultUrl =
  (import.meta as any).env?.VITE_FEED_URL || "ws://localhost:9002/stream";

export function useFeed(url = defaultUrl, maxPoints = 240) {
  const [status, setStatus] = useState<FeedStatus>("connecting");
  const [ticks, setTicks] = useState<Tick[]>([]);
  const [config, setConfig] = useState<any>(null);
  const [feedUrl] = useState(url || defaultUrl);
  const lastHeartbeat = useRef<number>(Date.now());

  useEffect(() => {
    const ws = new WebSocket(feedUrl);
    ws.onopen = () => {
      setStatus("live");
    };
    ws.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.type === "config") {
        setConfig(msg.data);
      } else if (msg.type === "tick") {
        setTicks((prev) => {
          const next = [...prev, msg];
          if (next.length > maxPoints) next.shift();
          return next;
        });
        const hasDrops = msg.drops_in_window && msg.drops_in_window > 0;
        setStatus(hasDrops ? "degraded" : "live");
        lastHeartbeat.current = Date.now();
      } else if (msg.type === "ping") {
        lastHeartbeat.current = Date.now();
      }
    };
    ws.onerror = () => setStatus("degraded");
    ws.onclose = () => setStatus("down");

    const heartbeat = setInterval(() => {
      if (Date.now() - lastHeartbeat.current > 5000) {
        setStatus("degraded");
      }
    }, 2000);

    return () => {
      ws.close();
      clearInterval(heartbeat);
    };
  }, [feedUrl, maxPoints]);

  return { status, ticks, config, feedUrl };
}
