type Props = {
  label: string;
  value: string;
  sub?: string;
  tone?: "good" | "bad" | "neutral";
};

export function KpiCard({ label, value, sub, tone = "neutral" }: Props) {
  const color =
    tone === "good" ? "#22d3ee" : tone === "bad" ? "#f97316" : "#e5e7eb";

  return (
    <div className="card" style={{ padding: "10px 12px" }}>
      <div style={{ color: "#94a3b8", fontSize: 12, letterSpacing: 0.4 }}>
        {label}
      </div>
      <div style={{ color, fontSize: 22, fontWeight: 600 }}>{value}</div>
      {sub && (
        <div style={{ color: "#94a3b8", fontSize: 12, marginTop: 2 }}>{sub}</div>
      )}
    </div>
  );
}
