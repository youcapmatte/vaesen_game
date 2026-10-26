import type { SocietyMember } from "@/common/api/society";

/** People on the call at once, the host among them; the Gamemaster is the extra seat. The API holds to it too. */
export const MAX_PLAYERS = 5;

/**
 * The seats at the table: the members on the call right now, in the order they joined it, at most five. Members who
 * are not on the call have no seat, and nobody else gets one (an account the host turned away is not a member).
 */
export function seatedOnCall(members: SocietyMember[], onCall: Iterable<string>): SocietyMember[] {
  const byAccount = new Map(members.map((m) => [m.accountId, m]));
  const seated: SocietyMember[] = [];
  for (const accountId of onCall) {
    const member = byAccount.get(accountId);
    if (member && !seated.includes(member)) seated.push(member);
  }
  return seated.slice(0, MAX_PLAYERS);
}
