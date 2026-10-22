import type { NextRequest } from "next/server";
import { NextResponse } from "next/server";
import { auth0 } from "@/common/server/auth0";

export const config = {
  matcher: ["/((?!_next/static|_next/image|favicon.ico|.*\\.(?:svg|png|jpg|jpeg|gif|webp|woff2?)).*)"],
};

/** Runs Auth0's routes (/auth/*) and keeps its session rolling. Without Auth0 configured it does nothing. */
export default async function proxy(request: NextRequest) {
  const response = auth0 ? await auth0.middleware(request) : NextResponse.next();

  response.headers.set("x-content-type-options", "nosniff");
  response.headers.set("x-frame-options", "SAMEORIGIN");
  return response;
}
