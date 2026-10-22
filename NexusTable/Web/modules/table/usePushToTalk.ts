"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { useTablePresence } from "@/common/client/signalr/useTable";

const SETTINGS_KEY = "nexus.pushToTalk";

export interface PushToTalkSettings {
  /** KeyboardEvent.code, e.g. "Backquote", or "Mouse3"/"Mouse4" for the mouse side buttons. */
  key: string;
  /** Toggle instead of hold. */
  toggle: boolean;
}

const DEFAULTS: PushToTalkSettings = { key: "Backquote", toggle: false };

export function keyLabel(code: string) {
  if (code === "Backquote") return "§ / `";
  if (code === "Mouse3") return "Mouse back";
  if (code === "Mouse4") return "Mouse forward";
  if (code.startsWith("Key")) return code.slice(3);
  if (code.startsWith("Digit")) return code.slice(5);
  return code;
}

function load(): PushToTalkSettings {
  try {
    const raw = window.localStorage.getItem(SETTINGS_KEY);
    return raw ? { ...DEFAULTS, ...(JSON.parse(raw) as PushToTalkSettings) } : DEFAULTS;
  } catch {
    return DEFAULTS;
  }
}

function isTyping(target: EventTarget | null) {
  const element = target as HTMLElement | null;
  return Boolean(element && (element.isContentEditable || ["INPUT", "TEXTAREA", "SELECT"].includes(element.tagName)));
}

/**
 * Talking to the Gamemaster: hold the key (or press it once in toggle mode). Only while this tab has focus,
 * a browser limit. The table sees who is talking; the audio will go to the Gamemaster through LiveKit.
 */
export function usePushToTalk() {
  const { setPushToTalk } = useTablePresence();
  const [settings, setSettings] = useState<PushToTalkSettings>(DEFAULTS);
  const [talking, setTalking] = useState(false);
  const talkingRef = useRef(false);

  useEffect(() => setSettings(load()), []);

  const set = useCallback((active: boolean) => {
    if (talkingRef.current === active) return;
    talkingRef.current = active;
    setTalking(active);
    setPushToTalk(active);
  }, [setPushToTalk]);

  useEffect(() => {
    const matches = (code: string) => code === settings.key;
    const down = (code: string, event: Event) => {
      if (!matches(code)) return;
      event.preventDefault();
      if (settings.toggle) set(!talkingRef.current);
      else set(true);
    };
    const up = (code: string) => {
      if (matches(code) && !settings.toggle) set(false);
    };

    const onKeyDown = (e: KeyboardEvent) => !e.repeat && !isTyping(e.target) && down(e.code, e);
    const onKeyUp = (e: KeyboardEvent) => up(e.code);
    const onMouseDown = (e: MouseEvent) => e.button >= 3 && down(`Mouse${e.button}`, e);
    const onMouseUp = (e: MouseEvent) => e.button >= 3 && up(`Mouse${e.button}`);
    // Letting go outside the tab never fires keyup: stop talking when the tab loses focus.
    const onBlur = () => !settings.toggle && set(false);

    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);
    window.addEventListener("mousedown", onMouseDown);
    window.addEventListener("mouseup", onMouseUp);
    window.addEventListener("blur", onBlur);
    return () => {
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
      window.removeEventListener("mousedown", onMouseDown);
      window.removeEventListener("mouseup", onMouseUp);
      window.removeEventListener("blur", onBlur);
    };
  }, [settings, set]);

  // Leaving the table while talking.
  useEffect(() => () => set(false), [set]);

  const save = useCallback((next: PushToTalkSettings) => {
    setSettings(next);
    try {
      window.localStorage.setItem(SETTINGS_KEY, JSON.stringify(next));
    } catch {
      // Kept for this visit only.
    }
  }, []);

  return { talking, settings, save, setTalking: set };
}
