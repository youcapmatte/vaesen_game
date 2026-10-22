"use client";

import { Badge, Box, Button, Center, Group, SimpleGrid, Stack, Text, Title, UnstyledButton } from "@mantine/core";
import { IconBrandDiscord } from "@tabler/icons-react";
import { useRouter } from "next/navigation";
import { useEffect, useState } from "react";
import { AccountApi, type DevPlayer } from "@/common/api/account";
import { useApiData } from "@/common/client/core/Api";
import { Corners, Divider, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader } from "@/common/client/components/PageFrame";
import { useSecurity } from "@/common/client/security/SecurityProvider";

/** The title page: sign in with Discord (Auth0), or as a seed player in development. */
export function LandingPage() {
  const { status, auth0Enabled, devLoginEnabled, signInDev } = useSecurity();
  const router = useRouter();
  const [error, setError] = useState<string>();
  const [signingIn, setSigningIn] = useState<string>();
  const { data: players, error: playersError } = useApiData<DevPlayer[]>(devLoginEnabled ? AccountApi.devPlayers : null, { anonymous: true });

  useEffect(() => {
    if (status === "signedIn") {
      router.replace("/society");
    }
  }, [status, router]);

  if (status !== "signedOut") {
    return <FullPageLoader />;
  }

  const devSignIn = async (playerId: string) => {
    setSigningIn(playerId);
    setError(await signInDev(playerId));
    setSigningIn(undefined);
  };

  return (
    <Center mih="100vh" p="md">
      <Box className="nx-panel" p={{ base: "xl", sm: 48 }} maw={640} w="100%">
        <Corners />
        <Stack align="center" gap="md" ta="center">
          <Text className="nx-kicker">Upsala · in the year of the Society&apos;s return</Text>
          <SightMark size={34} />
          <Title order={1} fz={{ base: 44, sm: 60 }} lh={1}>Nexus Table</Title>
          <Divider width={280} />
          <Text className="nx-flourish nx-dropcap" ta="left" fz="lg" c="var(--nx-parchment-2)" maw={480}>
            Only those born with the Sight can see them: the beings of the old stories, living on at the edges of
            the new century. Gather your Society around the table, light the candles, and let the Gamemaster
            tell you what waits in the dark.
          </Text>

          <Stack gap="xs" mt="md" w="100%" maw={360}>
            <Button
              component="a"
              href={auth0Enabled ? "/auth/login?returnTo=/society" : undefined}
              disabled={!auth0Enabled}
              size="lg"
              color="blood"
              leftSection={<IconBrandDiscord size={22} />}
            >
              Sign in with Discord
            </Button>
            {!auth0Enabled && (
              <Text fz="xs" c="var(--nx-sepia)">Discord sign-in opens once Auth0 is configured (see .env.example).</Text>
            )}
          </Stack>

          {devLoginEnabled && (
            <Stack gap="xs" mt="lg" w="100%">
              <Divider width={140} />
              <Text className="nx-kicker">Or take a seat as</Text>
              <ErrorNote message={error ?? (playersError ? "The API isn't answering. Start NexusTable/Api first." : undefined)} />
              <SimpleGrid cols={{ base: 2, sm: 3 }} spacing="xs">
                {players?.map((p) => (
                  <UnstyledButton
                    key={p.id}
                    onClick={() => devSignIn(p.id)}
                    disabled={signingIn !== undefined}
                    className="nx-parchment"
                    p="sm"
                    style={{ opacity: signingIn && signingIn !== p.id ? 0.5 : 1, transition: "transform 120ms", textAlign: "center" }}
                    onMouseEnter={(e) => (e.currentTarget.style.transform = "translateY(-2px) rotate(-0.5deg)")}
                    onMouseLeave={(e) => (e.currentTarget.style.transform = "none")}
                  >
                    <Group justify="center" gap={6}>
                      <Text className="nx-display" fz="lg">{p.name}</Text>
                      {p.isHost && <Badge size="xs" color="blood">Host</Badge>}
                    </Group>
                  </UnstyledButton>
                ))}
              </SimpleGrid>
            </Stack>
          )}
        </Stack>
      </Box>
    </Center>
  );
}
