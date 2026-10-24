"use client";

import { useLayoutEffect, useRef } from "react";
import styles from "./Dice.module.css";
import { playDieThrow } from "./diceSound";

/** Which pips are inked on each face, on a 3×3 grid (0 = top left, 8 = bottom right). */
const PIPS: Record<number, number[]> = {
  1: [4],
  2: [2, 6],
  3: [2, 4, 6],
  4: [0, 2, 6, 8],
  5: [0, 2, 4, 6, 8],
};

/** The cube's rotation that turns face N towards the viewer (faces: 1 front, 6 back, 3 right, 4 left, 2 top, 5 bottom). */
const FACE_ROTATION: Record<number, [number, number]> = {
  1: [0, 0],
  2: [-90, 0],
  3: [0, -90],
  4: [0, 90],
  5: [90, 0],
  6: [0, 180],
};

function Face({ value, className }: { value: number; className: string }) {
  return (
    <div className={`${styles.face} ${className}`}>
      {value === 6 ? (
        // The six is a success: the Sight star in blood red instead of pips.
        <svg viewBox="0 0 20 20" className={styles.star} aria-hidden>
          <path d="M10 0 L11.6 7 L18 3.5 L13 8.4 L20 10 L13 11.6 L18 16.5 L11.6 13 L10 20 L8.4 13 L2 16.5 L7 11.6 L0 10 L7 8.4 L2 3.5 L8.4 7 Z" />
          <circle cx="10" cy="10" r="2.4" className={styles.starEye} />
        </svg>
      ) : (
        Array.from({ length: 9 }, (_, i) => <span key={i} className={PIPS[value].includes(i) ? styles.pip : styles.noPip} />)
      )}
    </div>
  );
}

/** How long a throw takes, from leaving the hand to lying still. */
export const TUMBLE_MS = 2200;

const rand = (min: number, max: number) => min + Math.random() * (max - min);

/**
 * The keyframes of one throw: the die comes in from above and to one side, hits the table, bounces twice lower each
 * time, rolls on a little and rocks to rest on its face. The spin slows as it goes, like a real die losing speed.
 */
function throwFrames(size: number, rx: number, ry: number): Keyframe[] {
  const side = Math.random() < 0.5 ? -1 : 1;
  const startX = side * rand(1.5, 3) * size;
  const spin = [side * 360 * Math.round(rand(3, 5)), 360 * Math.round(rand(2, 4)) * (Math.random() < 0.5 ? -1 : 1), rand(-60, 60)];
  const bounce = [rand(0.7, 1), rand(0.25, 0.4)].map((h) => h * size);
  // offset: where in the throw; travel: how far across; height: above the table; spun: how much of the spin is done.
  const steps: { offset: number; travel: number; height: number; spun: number; easing: string }[] = [
    { offset: 0, travel: 0, height: size * 2.8, spun: 0, easing: "cubic-bezier(0.5, 0, 0.9, 0.6)" },
    { offset: 0.28, travel: 0.45, height: 0, spun: 0.5, easing: "cubic-bezier(0.2, 0.6, 0.5, 1)" },
    { offset: 0.44, travel: 0.68, height: bounce[0], spun: 0.7, easing: "cubic-bezier(0.5, 0, 0.8, 0.5)" },
    { offset: 0.58, travel: 0.82, height: 0, spun: 0.84, easing: "cubic-bezier(0.2, 0.6, 0.5, 1)" },
    { offset: 0.67, travel: 0.9, height: bounce[1], spun: 0.92, easing: "cubic-bezier(0.5, 0, 0.8, 0.5)" },
    { offset: 0.75, travel: 0.95, height: 0, spun: 0.97, easing: "ease-out" },
    { offset: 0.88, travel: 1, height: 0, spun: 1.01, easing: "ease-in-out" },
    { offset: 1, travel: 1, height: 0, spun: 1, easing: "linear" },
  ];
  return steps.map((s) => {
    const left = 1 - s.spun;
    return {
      offset: s.offset,
      easing: s.easing,
      transform:
        `translate(${startX * (1 - s.travel)}px, ${-s.height}px) ` +
        `rotateX(${rx - 12 + spin[0] * left}deg) rotateY(${ry + 14 + spin[1] * left}deg) rotateZ(${spin[2] * left}deg)`,
    };
  });
}

/**
 * When the throw's die strikes the table, as offsets of the throw (they match <see cref="throwFrames"/>): the landing,
 * two bounces, then a few small clicks as it rolls on and rocks to rest.
 */
const THROW_IMPACTS = () => [
  { offset: 0.28, strength: rand(0.85, 1) },
  { offset: 0.58, strength: rand(0.45, 0.6) },
  { offset: 0.75, strength: rand(0.25, 0.35) },
  ...Array.from({ length: 3 + Math.floor(Math.random() * 3) }, (_, i) => ({ offset: 0.78 + i * rand(0.025, 0.035), strength: rand(0.06, 0.14) })),
];

/**
 * A bone die thrown onto <paramref name="value"/>. Change <paramref name="throwKey"/> to throw it again.
 * The value comes from the server; the throw is only for show.
 */
export function Die({ value, size = 44, throwKey, delay = 0, dim }: { value: number; size?: number; throwKey: string; delay?: number; dim?: boolean }) {
  const cube = useRef<HTMLDivElement>(null);
  const [rx, ry] = FACE_ROTATION[value] ?? [0, 0];
  const rest = `rotateX(${rx - 12}deg) rotateY(${ry + 14}deg)`;

  // Before paint, so the face it lands on never shows before the throw starts.
  useLayoutEffect(() => {
    const el = cube.current;
    if (!el) return;
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
      return playDieThrow(delay, TUMBLE_MS, [{ offset: 0, strength: 0.5 }]);
    }

    const animation = el.animate(throwFrames(size, rx, ry), { duration: TUMBLE_MS, delay, fill: "backwards" });
    const silence = playDieThrow(delay, TUMBLE_MS, THROW_IMPACTS());
    return () => {
      animation.cancel();
      silence();
    };
    // Thrown again only when the throw changes; a new value always comes with a new throw.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [throwKey, delay]);

  return (
    <div className={styles.scene} style={{ width: size, height: size, ["--die-size" as string]: `${size}px`, opacity: dim ? 0.45 : 1 }}>
      <div ref={cube} className={styles.cube} style={{ transform: rest }}>
        <Face value={1} className={styles.front} />
        <Face value={6} className={styles.back} />
        <Face value={3} className={styles.right} />
        <Face value={4} className={styles.left} />
        <Face value={2} className={styles.top} />
        <Face value={5} className={styles.bottom} />
      </div>
    </div>
  );
}

/** A flat die face for the log: small, no animation. */
export function DieFace({ value, size = 20, faded }: { value: number; size?: number; faded?: boolean }) {
  return (
    <div className={styles.flat} style={{ width: size, height: size, ["--die-size" as string]: `${size}px`, opacity: faded ? 0.4 : 1 }}>
      <Face value={value} className={styles.flatFace} />
    </div>
  );
}
