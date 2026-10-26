import { noteBody, sessionStamp } from "./session";

describe("the session clock", () => {
  it("reads as hours, minutes and seconds since the session began", () => {
    expect(sessionStamp(2515)).toBe("0:41:55");
    expect(sessionStamp(3 * 3600 + 7.9)).toBe("3:00:07");
    expect(sessionStamp(-4)).toBe("0:00:00");
  });

  it("sends only how long ago a note was begun, so a wrong computer clock cannot misplace it", () => {
    expect(noteBody("Elin took the key.", 1_000, 91_500)).toEqual({ text: "Elin took the key.", secondsAgo: 90.5 });
    expect(noteBody("We rest.", undefined, 5_000)).toEqual({ text: "We rest.", secondsAgo: 0 });
  });
});
