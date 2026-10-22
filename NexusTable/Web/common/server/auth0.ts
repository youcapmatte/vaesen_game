import { Auth0Client } from "@auth0/nextjs-auth0/server";

/** Auth0 is on once the server has its tenant settings; until then the app runs with the dev login only. */
export const auth0Enabled = Boolean(process.env.AUTH0_DOMAIN && process.env.AUTH0_CLIENT_ID && process.env.AUTH0_SECRET);

/**
 * Auth0 with the Discord social connection. Routes: /auth/login, /auth/logout, /auth/callback, /auth/profile,
 * /auth/access-token. The access token is for the Nexus Table API (AUTH0_AUDIENCE).
 */
export const auth0: Auth0Client | undefined = auth0Enabled
  ? new Auth0Client({
      appBaseUrl: process.env.APP_BASE_URL,
      logoutStrategy: "v2",
      authorizationParameters: {
        scope: "openid profile email offline_access",
        audience: process.env.AUTH0_AUDIENCE,
        // Straight to Discord instead of Auth0's login page.
        connection: process.env.AUTH0_CONNECTION || "discord",
      },
    })
  : undefined;
