"use client";

import { Badge, Button, Group, Stack } from "@mantine/core";
import { IconCandle } from "@tabler/icons-react";
import { useState } from "react";
import { MysteryApi } from "@/common/api/mystery";
import type { Society } from "@/common/api/society";
import { Http, useApi } from "@/common/client/core/Api";
import { useSociety } from "@/common/client/components/hooks";
import { ErrorNote } from "@/common/client/components/PageFrame";

/**
 * Puts a case on the table, or takes it off. On the table, players see its title and hook letter, and the Gamemaster
 * runs it the next time the host brings it in.
 */
export function TableButton({ mysteryId, playable }: { mysteryId: string; playable: boolean }) {
  const call = useApi();
  const { data: view, mutate } = useSociety();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();
  const atTable = view?.society.atTable;
  const here = atTable?.mysteryId === mysteryId;
  if (!view || (!here && !playable)) {
    return null;
  }

  const toggle = async () => {
    setBusy(true);
    setError(undefined);
    const result = here
      ? await call<Society>(Http.DELETE, MysteryApi.clearTable)
      : await call<Society>(Http.POST, MysteryApi.table(mysteryId));
    setBusy(false);
    if (!result.success) {
      setError(result.errorMessage);
      return;
    }
    await mutate();
  };

  return (
    <Stack gap={4} align="center">
      {here ? (
        <Group gap="xs" justify="center">
          <Badge color="gold" leftSection={<IconCandle size={12} />}>At the table</Badge>
          <Button size="xs" variant="subtle" color="umber" onClick={toggle} loading={busy}>Take it off the table</Button>
        </Group>
      ) : (
        <Button size="xs" color="gold" leftSection={<IconCandle size={14} />} onClick={toggle} loading={busy}>
          {atTable ? `Bring to the table instead of “${atTable.title}”` : "Bring to the table"}
        </Button>
      )}
      <ErrorNote message={error} />
    </Stack>
  );
}
