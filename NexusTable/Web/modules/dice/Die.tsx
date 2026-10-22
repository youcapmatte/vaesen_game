"use client";

import { useEffect, useState } from "react";
import styles from "./Dice.module.css";

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

/**
 * A bone die that tumbles onto <paramref name="value"/>. Change <paramref name="throwKey"/> to throw it again.
 * The value comes from the server; the tumble is only for show.
 */
export function Die({ value, size = 44, throwKey, delay = 0, dim }: { value: number; size?: number; throwKey: string; delay?: number; dim?: boolean }) {
  const [landed, setLanded] = useState(false);
  const [spin] = useState(() => [360 * (2 + Math.floor(Math.random() * 2)), 360 * (2 + Math.floor(Math.random() * 2)), Math.random() * 40 - 20]);

  useEffect(() => {
    setLanded(false);
    const id = window.setTimeout(() => setLanded(true), 30 + delay);
    return () => window.clearTimeout(id);
  }, [throwKey, delay]);

  const [rx, ry] = FACE_ROTATION[value] ?? [0, 0];
  const transform = landed
    ? `rotateX(${rx - 12}deg) rotateY(${ry + 14}deg)`
    : `translateY(-${size * 2.5}px) rotateX(${rx + spin[0]}deg) rotateY(${ry + spin[1]}deg) rotateZ(${spin[2]}deg)`;

  return (
    <div className={styles.scene} style={{ width: size, height: size, ["--die-size" as string]: `${size}px`, opacity: dim ? 0.45 : 1 }}>
      <div
        className={styles.cube}
        style={{ transform, transition: landed ? `transform 1100ms cubic-bezier(0.18, 0.9, 0.25, 1.05) ${delay}ms` : "none" }}
      >
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
