/** Hand-drawn ornaments in the spirit of the rulebook's chapter rules. Colour follows currentColor. */

export function Divider({ width = 260, className, style }: { width?: number; className?: string; style?: React.CSSProperties }) {
  return (
    <svg
      viewBox="0 0 260 18"
      width={width}
      height={(width / 260) * 18}
      aria-hidden
      className={className}
      style={{ display: "block", margin: "0 auto", color: "var(--nx-gold)", ...style }}
    >
      <g fill="none" stroke="currentColor" strokeWidth="1" strokeLinecap="round">
        <path d="M4 9 H104" />
        <path d="M156 9 H256" />
        <path d="M104 9 C112 2, 120 2, 124 9 C120 16, 112 16, 104 9 Z" />
        <path d="M156 9 C148 2, 140 2, 136 9 C140 16, 148 16, 156 9 Z" />
        <circle cx="130" cy="9" r="3.2" />
        <path d="M130 1.5 V4.5 M130 13.5 V16.5" />
      </g>
      <circle cx="130" cy="9" r="1.3" fill="currentColor" />
      <circle cx="4" cy="9" r="1.4" fill="currentColor" />
      <circle cx="256" cy="9" r="1.4" fill="currentColor" />
    </svg>
  );
}

/** A small eight-pointed star, the "Sight" mark used for bullets and empty states. */
export function SightMark({ size = 14, style }: { size?: number; style?: React.CSSProperties }) {
  return (
    <svg viewBox="0 0 20 20" width={size} height={size} aria-hidden style={{ color: "var(--nx-gold)", flex: "none", ...style }}>
      <path
        d="M10 0 L11.6 7 L18 3.5 L13 8.4 L20 10 L13 11.6 L18 16.5 L11.6 13 L10 20 L8.4 13 L2 16.5 L7 11.6 L0 10 L7 8.4 L2 3.5 L8.4 7 Z"
        fill="currentColor"
        opacity="0.85"
      />
      <circle cx="10" cy="10" r="2.2" fill="var(--nx-night)" />
    </svg>
  );
}

/** Corner flourishes for a framed panel; place inside a position: relative box. */
export function Corners({ inset = 6, color = "var(--nx-gold)" }: { inset?: number; color?: string }) {
  const corner = (rotate: number, pos: React.CSSProperties) => (
    <svg viewBox="0 0 24 24" width="22" height="22" aria-hidden style={{ position: "absolute", color, transform: `rotate(${rotate}deg)`, pointerEvents: "none", ...pos }}>
      <path d="M1 23 V8 C1 4, 4 1, 8 1 H23" fill="none" stroke="currentColor" strokeWidth="1" />
      <path d="M5 23 V10 C5 7, 7 5, 10 5 H23" fill="none" stroke="currentColor" strokeWidth="0.6" opacity="0.6" />
      <circle cx="8" cy="8" r="1.4" fill="currentColor" />
    </svg>
  );
  return (
    <>
      {corner(0, { top: inset, left: inset })}
      {corner(90, { top: inset, right: inset })}
      {corner(180, { bottom: inset, right: inset })}
      {corner(270, { bottom: inset, left: inset })}
    </>
  );
}
