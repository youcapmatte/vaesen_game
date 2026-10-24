"use client";

/**
 * The sound of bone dice on a wooden table, made in the browser (Web Audio), so no recording is fetched or licensed.
 * Each impact is a short bright click (the die's edge) over a soft low knock (the table), and a die rolling to rest
 * is a run of small clicks. Each die schedules its own impacts at its own bounces, so a handful of dice clatter.
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

/** One impact at <paramref name="at"/> (AudioContext time) with a strength of 0-1. */
function knock(ctx: AudioContext, at: number, strength: number): AudioScheduledSourceNode[] {
  // The click: a few milliseconds of noise through a bright band-pass, as bone edges on wood.
  const click = ctx.createBufferSource();
  click.buffer = noise;
  click.playbackRate.value = rand(0.85, 1.25);
  const band = ctx.createBiquadFilter();
  band.type = "bandpass";
  band.frequency.value = rand(2200, 4800);
  band.Q.value = rand(1.2, 2.4);
  const clickGain = ctx.createGain();
  clickGain.gain.setValueAtTime(0.0001, at);
  clickGain.gain.exponentialRampToValueAtTime(0.9 * strength, at + 0.002);
  clickGain.gain.exponentialRampToValueAtTime(0.0001, at + rand(0.03, 0.06));
  click.connect(band).connect(clickGain).connect(output!);
  click.start(at, rand(0, 0.8), 0.08);

  // The table answering: a short low knock that dies fast.
  const body = ctx.createOscillator();
  body.type = "sine";
  body.frequency.setValueAtTime(rand(150, 210), at);
  body.frequency.exponentialRampToValueAtTime(rand(80, 110), at + 0.08);
  const bodyGain = ctx.createGain();
  bodyGain.gain.setValueAtTime(0.0001, at);
  bodyGain.gain.exponentialRampToValueAtTime(0.5 * strength, at + 0.003);
  bodyGain.gain.exponentialRampToValueAtTime(0.0001, at + 0.1);
  body.connect(bodyGain).connect(output!);
  body.start(at);
  body.stop(at + 0.12);
  return [click, body];
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
  const sources: AudioScheduledSourceNode[] = [];
  for (const { offset, strength } of impacts) {
    const at = start + (offset * duration) / 1000;
    sources.push(...knock(ctx, at, strength));
    // A die rarely lands flat: the second edge touches a moment later.
    if (strength > 0.3) sources.push(...knock(ctx, at + rand(0.008, 0.02), strength * rand(0.25, 0.45)));
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
