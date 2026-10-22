"use client";

import { Button, Container, Group } from "@mantine/core";
import { IconArrowLeft } from "@tabler/icons-react";
import Link from "next/link";
import { CharacterApi, type Character } from "@/common/api/character";
import { useApiData } from "@/common/client/core/Api";
import { useMe, useRules } from "@/common/client/components/hooks";
import { ErrorNote, FullPageLoader, PageFrame } from "@/common/client/components/PageFrame";
import { useTableEvent } from "@/common/client/signalr/useTable";
import { CharacterSheet } from "./CharacterSheet";

export function CharacterSheetPage({ characterId }: { characterId: string }) {
  return (
    <PageFrame>
      <SheetContent characterId={characterId} />
    </PageFrame>
  );
}

function SheetContent({ characterId }: { characterId: string }) {
  const { data: rules } = useRules();
  const { data: me } = useMe();
  const { data: character, error, mutate } = useApiData<Character>(CharacterApi.character(characterId));
  // Broadcasts leave out the dark secret; keep the one this viewer was allowed to read.
  useTableEvent("characterChanged", (changed) => changed.id === characterId
    && void mutate((current) => ({ ...changed, darkSecret: changed.darkSecret || current?.darkSecret || "" }), { revalidate: false }));

  if (error) {
    return <Container size="sm"><ErrorNote message={error.message} /></Container>;
  }
  if (!rules || !character || !me) {
    return <FullPageLoader label="Unfolding the sheet…" />;
  }

  const canEdit = character.accountId === me.id || me.role === "Host";
  return (
    <Container size="lg">
      <Group mb="md">
        <Button component={Link} href="/society" variant="subtle" color="gold" leftSection={<IconArrowLeft size={16} />}>The Society</Button>
      </Group>
      <CharacterSheet
        character={character}
        rules={rules}
        canEdit={canEdit}
        canSeeSecret={canEdit}
        onChange={(c) => void mutate(c, { revalidate: false })}
      />
    </Container>
  );
}
