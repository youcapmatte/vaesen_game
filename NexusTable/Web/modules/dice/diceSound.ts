"use client";

/**
 * The sound of bone dice on a wooden table, made in the browser (Web Audio), so no recording is fetched or licensed.
 * Each impact is a sharp tick (the die's edge) with a short woody ring and a faint tap from the table, and a die
 * rolling to rest is a run of small clicks. Each die schedules its own impacts at its own bounces, so a handful of dice clatter.
 */

const STORAGE_KEY = "nx-dice-sound";

let context: AudioContext | null = null;
let output: AudioNode | null = null;
let noise: AudioBuffer | null = null;

/** Whether the viewer wants the sound; on unless they turned it off. */
export function diceSoundOn(): boolean {
  try {
    return window.localStorage.getItem(STORAGE_KEY) !== "off";
  } catch {
    return true;
  }
}

export function setDiceSound(on: boolean) {
  try {
    window.localStorage.setItem(STORAGE_KEY, on ? "on" : "off");
  } catch {
    // A private window without storage: the choice lasts until the page is left.
  }
}

/** The audio graph, made on first use (browsers allow sound only after the viewer has interacted with the page). */
function audio(): AudioContext | null {
  if (typeof window === "undefined" || !("AudioContext" in window)) return null;
  if (!context) {
    context = new AudioContext();
    const compressor = context.createDynamicsCompressor();
    compressor.threshold.value = -18;
    compressor.ratio.value = 6;
    const master = context.createGain();
    master.gain.value = 0.7;
    master.connect(compressor).connect(context.destination);
    output = master;
    noise = context.createBuffer(1, context.sampleRate, context.sampleRate);
    const data = noise.getChannelData(0);
    for (let i = 0; i < data.length; i++) data[i] = Math.random() * 2 - 1;
  }
  if (context.state === "suspended") void context.resume();
  return context;
}

const rand = (min: number, max: number) => min + Math.random() * (max - min);

/** A burst of noise from the shared buffer, shaped by a gain envelope that starts at <paramref name="at"/>. */
function burst(ctx: AudioContext, at: number, length: number, rate: number): AudioBufferSourceNode {
  const source = ctx.createBufferSource();
  source.buffer = noise;
  source.playbackRate.value = rate;
  source.start(at, rand(0, 0.9), length);
  return source;
}

function envelope(ctx: AudioContext, at: number, peak: number, decay: number): GainNode {
  const gain = ctx.createGain();
  gain.gain.setValueAtTime(0.0001, at);
  gain.gain.exponentialRampToValueAtTime(Math.max(0.0002, peak), at + 0.0008);
  gain.gain.exponentialRampToValueAtTime(0.0001, at + decay);
  return gain;
}

/**
 * One impact at <paramref name="at"/> (AudioContext time) with a strength of 0-1, for a die of the given
 * <paramref name="pitch"/>. A die on wood is a sharp click with a short, bright, woody ring and almost no low end:
 * a falling low tone is what makes a thrown thing sound like a rubber ball.
 */
function knock(ctx: AudioContext, at: number, strength: number, pitch: number): AudioScheduledSourceNode[] {
  const sources: AudioScheduledSourceNode[] = [];

  // The edge: a millisecond-short, very bright tick.
  const tick = burst(ctx, at, 0.01, rand(1.2, 1.6));
  const highpass = ctx.createBiquadFilter();
  highpass.type = "highpass";
  highpass.frequency.value = 3500;
  tick.connect(highpass).connect(envelope(ctx, at, 0.7 * strength, rand(0.008, 0.014))).connect(output!);
  sources.push(tick);

  // Bone and board: a few narrow resonances that ring for a few dozen milliseconds, no pitch sweep.
  const modes = [1250, 2350, 3900].map((f) => f * pitch * rand(0.97, 1.03));
  const excite = burst(ctx, at, 0.012, 1);
  modes.forEach((frequency, i) => {
    const band = ctx.createBiquadFilter();
    band.type = "bandpass";
    band.frequency.value = frequency;
    band.Q.value = rand(14, 24);
    excite.connect(band).connect(envelope(ctx, at, [2.6, 1.8, 1.1][i] * strength, rand(0.035, 0.07) / (1 + i * 0.3))).connect(output!);
  });
  sources.push(excite);

  // The table: a faint, dull tap, low-passed and gone at once.
  const tap = burst(ctx, at, 0.02, 0.6);
  const lowpass = ctx.createBiquadFilter();
  lowpass.type = "lowpass";
  lowpass.frequency.value = 700;
  tap.connect(lowpass).connect(envelope(ctx, at, 0.35 * strength, 0.025)).connect(output!);
  sources.push(tap);
  return sources;
}

/**
 * The sound of one die's throw, its impacts at the given offsets (0-1) of the throw's duration. Call it when the die is
 * thrown; <paramref name="delay"/> and <paramref name="duration"/> are in milliseconds.
 */
export function playDieThrow(delay: number, duration: number, impacts: { offset: number; strength: number }[]): () => void {
  const silence = () => {};
  if (!diceSoundOn()) return silence;
  const ctx = audio();
  if (!ctx || !output || !noise) return silence;
  const start = ctx.currentTime + delay / 1000;
  // Each die rings a little higher or lower than the next, as no two bones are alike.
  const pitch = rand(0.85, 1.18);
  const sources: AudioScheduledSourceNode[] = [];
  for (const { offset, strength } of impacts) {
    const at = start + (offset * duration) / 1000;
    sources.push(...knock(ctx, at, strength, pitch));
    // A die rarely lands flat: its other corners touch a moment later, quieter each time.
    if (strength > 0.3) {
      let corner = at;
      for (let i = 0; i < 1 + Math.floor(Math.random() * 2); i++) {
        corner += rand(0.007, 0.022);
        sources.push(...knock(ctx, corner, strength * rand(0.2, 0.4) / (i + 1), pitch));
      }
    }
  }

  // Stops what has not sounded yet: the throw was replaced, or the die left the page.
  return () => {
    for (const source of sources) {
      try {
        source.stop();
      } catch {
        // Already finished.
      }
    }
  };
}
