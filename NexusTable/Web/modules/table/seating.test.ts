import type { CaseSummary } from "@/common/api/mystery";
import { playableCases } from "@/common/api/mystery";
import type { SocietyMember } from "@/common/api/society";
import { MAX_PLAYERS, seatedOnCall } from "./seating";

const member = (n: number): SocietyMember => ({ accountId: `account-${n}`, displayName: `Player ${n}`, role: "Player" });

describe("the seats at the table", () => {
  it("are the members on the call right now, in the order they joined it", () => {
    const members = [member(1), member(2), member(3)];

    // Member 2 signed in once but is not on the call: no seat, no empty chair.
    expect(seatedOnCall(members, ["account-3", "account-1"]).map((m) => m.accountId)).toEqual(["account-3", "account-1"]);
  });

  it("give nobody a seat who is not a member, and hold at most five", () => {
    const members = Array.from({ length: 7 }, (_, i) => member(i + 1));

    expect(seatedOnCall(members, ["account-turned-away", "account-1"]).map((m) => m.accountId)).toEqual(["account-1"]);
    expect(seatedOnCall(members, members.map((m) => m.accountId))).toHaveLength(MAX_PLAYERS);
    expect(MAX_PLAYERS).toBe(5);
  });
});

describe("the cases the host can bring in from the lobby", () => {
  it("are those that passed and are not closed", () => {
    const base = { societyId: "s", generatedAt: "2026-10-01", levelId: "l", month: 5, vaesenIds: [], hookChannelId: "c1", hookGiverType: "x" };
    const cases: CaseSummary[] = [
      { ...base, mysteryId: "ready", title: "Ready", passed: true, status: "generated" },
      { ...base, mysteryId: "solved", title: "Solved", passed: true, status: "solved" },
      { ...base, mysteryId: "rejected", title: "Rejected", passed: false, status: "rejected" },
    ];

    expect(playableCases(cases).map((c) => c.mysteryId)).toEqual(["ready"]);
  });
});
