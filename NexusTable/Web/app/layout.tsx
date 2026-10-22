import "@mantine/core/styles.css";
import "@mantine/notifications/styles.css";
import "@/common/styles/global.css";

import { ColorSchemeScript, MantineProvider, mantineHtmlProps } from "@mantine/core";
import { Notifications } from "@mantine/notifications";
import type { Metadata } from "next";
import { EB_Garamond, IM_Fell_English, IM_Fell_English_SC } from "next/font/google";
import { connection } from "next/server";
import { SecurityProvider } from "@/common/client/security/SecurityProvider";
import { TableConnectionProvider } from "@/common/client/signalr/useTable";
import { auth0, auth0Enabled } from "@/common/server/auth0";
import { theme } from "@/common/styles/theme";

const display = IM_Fell_English_SC({ weight: "400", subsets: ["latin"], variable: "--font-display", display: "swap" });
const flourish = IM_Fell_English({ weight: "400", style: ["normal", "italic"], subsets: ["latin"], variable: "--font-flourish", display: "swap" });
const body = EB_Garamond({ subsets: ["latin", "latin-ext"], variable: "--font-body", display: "swap" });

export const metadata: Metadata = {
  title: "Nexus Table",
  description: "A Vaesen mystery around a candlelit table, with Claude as the Gamemaster.",
};

export default async function RootLayout({ children }: { children: React.ReactNode }) {
  // Render per request: the Auth0 settings and the session are read when the server runs, not when the image is built.
  await connection();
  const session = auth0 ? await auth0.getSession() : null;
  const devLoginEnabled = process.env.NEXT_PUBLIC_DEV_LOGIN === "true";

  return (
    <html lang="en" {...mantineHtmlProps} className={`${display.variable} ${flourish.variable} ${body.variable}`}>
      <head>
        <ColorSchemeScript forceColorScheme="dark" />
      </head>
      <body>
        <MantineProvider theme={theme} forceColorScheme="dark">
          <Notifications position="top-center" />
          <SecurityProvider auth0Enabled={auth0Enabled} devLoginEnabled={devLoginEnabled} user={session?.user}>
            <TableConnectionProvider>{children}</TableConnectionProvider>
          </SecurityProvider>
        </MantineProvider>
      </body>
    </html>
  );
}
