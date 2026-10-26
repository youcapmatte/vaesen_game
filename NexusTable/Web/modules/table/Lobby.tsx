"use client";

import { Anchor, Badge, Box, Button, Group, Stack, Text, Title } from "@mantine/core";
import { IconCandle, IconScript } from "@tabler/icons-react";
import Link from "next/link";
import { useState } from "react";
import { type CaseSummary, MysteryApi, playableCases } from "@/common/api/mystery";
import type { Society } from "@/common/api/society";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { Divider } from "@/common/client/components/Ornament";
import { ErrorNote } from "@/common/client/components/PageFrame";

/**
 * The lobby: the table before a case is on it. Everyone on the call talks freely and nothing goes to the Gamemaster.
 * The host chooses a case here and brings it to the table; the page then becomes the mystery for everyone, on the
 * same call.
 */
export function Lobby({ headquarters, isHost, onBrought }: { headquarters: string; isHost: boolean; onBrought: () => void }) {
  return (
    <Stack align="center" gap="sm" maw={620} ta="center" p="xl" w="100%">
      <Text className="nx-kicker">The lobby · {headquarters}</Text>
      <Title order={2}>The Society gathers</Title>
      <Divider width={200} />
      <Text className="nx-flourish nx-dropcap" ta="left" c="var(--nx-parchment-2)">
        Pull up a chair. Everyone on the call can talk freely here; the Gamemaster isn&apos;t listening yet.
        {isHost
          ? " When you are ready, choose a case and bring it to the table: the lobby becomes the mystery for everyone, without leaving the call."
          : " When the host brings a case to the table, the lobby becomes the mystery, without leaving the call."}
      </Text>
      {isHost && <CasePicker onBrought={onBrought} />}
    </Stack>
  );
}

function CasePicker({ onBrought }: { onBrought: () => void }) {
  const call = useApi();
  const { data: cases, error } = useApiData<CaseSummary[]>(MysteryApi.list);
  const [busy, setBusy] = useState<string>();
  const [failed, setFailed] = useState<string>();

  const bring = async (mysteryId: string) => {
    setBusy(mysteryId);
    const result = await call<Society>(Http.POST, MysteryApi.table(mysteryId));
    setBusy(undefined);
    setFailed(result.success ? undefined : result.errorMessage);
    if (result.success) onBrought();
  };

  if (error) return <ErrorNote message={error.message} />;
  if (!cases) return <Text fz="sm" c="var(--nx-sepia)">Reading the case files…</Text>;

  const ready = playableCases(cases);
  return (
    <Box className="nx-panel" p="md" w="100%" ta="left">
      <Group justify="space-between" mb="xs">
        <Text className="nx-kicker">Cases ready to play</Text>
        <Anchor component={Link} href="/mysteries" fz="sm" c="gold.4">
          <Group gap={4} component="span"><IconScript size={14} />The case files</Group>
        </Anchor>
      </Group>
      {ready.length === 0 ? (
        <Text fz="sm" c="var(--nx-sepia)">No case is ready yet. Write one in the case files.</Text>
      ) : (
        <Stack gap={6}>
          {ready.map((c) => (
            <Group key={c.mysteryId} justify="space-between" wrap="nowrap" gap="sm">
              <Box style={{ minWidth: 0 }}>
                <Text className="nx-display" fz="md" truncate>{c.title || "Untitled"}</Text>
                <Group gap={6}>
                  <Text fz={11} c="var(--nx-sepia)">Written {new Date(c.generatedAt).toLocaleDateString()}</Text>
                  {c.needsReview && <Badge size="xs" color="blood" variant="outline">Read before playing</Badge>}
                </Group>
              </Box>
              <Button size="compact-sm" color="gold" leftSection={<IconCandle size={14} />} loading={busy === c.mysteryId} disabled={Boolean(busy)} onClick={() => void bring(c.mysteryId)}>
                Bring it to the table
              </Button>
            </Group>
          ))}
        </Stack>
      )}
      <ErrorNote message={failed} />
    </Box>
  );
}
