"use client";

import { Avatar, Badge, Box, Button, Container, Group, Modal, SimpleGrid, Stack, Text, TextInput, Title } from "@mantine/core";
import { useDisclosure } from "@mantine/hooks";
import { IconCandle, IconFeather, IconPencil } from "@tabler/icons-react";
import Link from "next/link";
import { useState } from "react";
import { SocietyApi, type Society, type SocietyMember } from "@/common/api/society";
import { Http, useApi } from "@/common/client/core/Api";
import { ruleNames, useMe, useRules, useSociety } from "@/common/client/components/hooks";
import { Corners, SightMark } from "@/common/client/components/Ornament";
import { ErrorNote, FullPageLoader, PageFrame, PageTitle } from "@/common/client/components/PageFrame";

export function SocietyHall() {
  return (
    <PageFrame>
      <HallContent />
    </PageFrame>
  );
}

function HallContent() {
  const { data: view, error, mutate } = useSociety();
  const { data: me } = useMe();
  const { data: rules } = useRules();
  const [editing, { open, close }] = useDisclosure(false);

  if (error) {
    return <Container size="sm"><ErrorNote message={error.message} /></Container>;
  }
  if (!view || !me) {
    return <FullPageLoader label="Opening the doors of the Society…" />;
  }

  const names = ruleNames(rules);
  const mine = view.members.find((m) => m.accountId === me.id);
  const isHost = me.role === "Host";

  return (
    <Container size="lg">
      <PageTitle kicker="The Society" title={view.society.name}>
        {view.society.headquarters}
      </PageTitle>

      <Group justify="center" gap="sm" mb="xl">
        <Button component={Link} href="/table" size="md" leftSection={<IconCandle size={18} />}>Take your seat at the table</Button>
        {isHost && <Button variant="outline" color="gold" leftSection={<IconPencil size={16} />} onClick={open}>Edit the Society</Button>}
      </Group>

      {!mine?.character && (
        <Box className="nx-panel" p="xl" mb="xl" ta="center">
          <Corners />
          <Stack align="center" gap="xs">
            <SightMark size={22} />
            <Title order={3}>You have the Sight, but no name yet</Title>
            <Text className="nx-flourish" c="var(--nx-parchment-2)" maw={520}>
              Before the Society sends you north, tell us who you are: your calling, what you saw, and what you hide.
            </Text>
            <Button component={Link} href="/characters/new" mt="sm" color="blood" leftSection={<IconFeather size={18} />}>
              Create your character
            </Button>
          </Stack>
        </Box>
      )}

      <Text className="nx-kicker" ta="center" mb="sm">Members · {view.members.length} of 4 seats</Text>
      <SimpleGrid cols={{ base: 1, sm: 2, md: 4 }} spacing="lg">
        {view.members.map((member) => (
          <MemberCard key={member.accountId} member={member} isMe={member.accountId === me.id} archetypeName={names.archetype(member.character?.archetype)} />
        ))}
        {Array.from({ length: Math.max(0, 4 - view.members.length) }, (_, i) => (
          <Box key={`empty-${i}`} p="lg" style={{ border: "1px dashed var(--nx-gold-dim)", minHeight: 200, display: "grid", placeItems: "center" }}>
            <Text className="nx-flourish" c="var(--nx-sepia)">An empty chair</Text>
          </Box>
        ))}
      </SimpleGrid>

      <EditSocietyModal opened={editing} onClose={close} society={view.society} onSaved={() => void mutate()} />
    </Container>
  );
}

function MemberCard({ member, isMe, archetypeName }: { member: SocietyMember; isMe: boolean; archetypeName: string }) {
  const character = member.character;
  const conditions = character?.conditions.length ?? 0;
  const body = (
    <Stack gap={8} align="center" ta="center" h="100%">
      <Avatar src={character?.portraitUrl ?? member.avatarUrl} size={72} radius="50%" color="umber" style={{ border: "2px solid rgba(43,33,24,0.5)", filter: "sepia(0.6) contrast(1.05)" }}>
        {(character?.name ?? member.displayName)[0]}
      </Avatar>
      {character ? (
        <>
          <Title order={4}>{character.name}</Title>
          <Text className="nx-kicker" c="var(--nx-blood)" fz={11}>{archetypeName}</Text>
          <Text fz="sm" c="var(--nx-ink-soft)" fs="italic" lineClamp={2}>{character.motivation}</Text>
        </>
      ) : (
        <Text className="nx-flourish" c="var(--nx-ink-soft)" mt="sm">Has not yet chosen who to be.</Text>
      )}
      <Group gap={6} mt="auto" justify="center">
        <Text fz="xs" c="var(--nx-ink-soft)">played by {member.displayName}</Text>
        {member.role === "Host" && <Badge size="xs" color="blood">Host</Badge>}
        {isMe && <Badge size="xs" color="umber.6">You</Badge>}
        {conditions > 0 && <Badge size="xs" color="blood.8" variant="outline">{conditions} Condition{conditions > 1 ? "s" : ""}</Badge>}
      </Group>
    </Stack>
  );

  return character ? (
    <Box component={Link} href={`/characters/${character.id}`} className="nx-parchment" p="lg" style={{ textDecoration: "none", minHeight: 200, display: "block" }}>
      {body}
    </Box>
  ) : (
    <Box className="nx-parchment" p="lg" style={{ minHeight: 200, opacity: 0.85 }}>{body}</Box>
  );
}

function EditSocietyModal({ opened, onClose, society, onSaved }: { opened: boolean; onClose: () => void; society: Society; onSaved: () => void }) {
  const call = useApi();
  const [name, setName] = useState(society.name);
  const [headquarters, setHeadquarters] = useState(society.headquarters);
  const [error, setError] = useState<string>();
  const [saving, setSaving] = useState(false);

  const save = async () => {
    setSaving(true);
    const result = await call<Society>(Http.PATCH, SocietyApi.society, { name, headquarters });
    setSaving(false);
    if (!result.success) {
      setError(result.errorMessage);
      return;
    }
    onSaved();
    onClose();
  };

  return (
    <Modal opened={opened} onClose={onClose} title={<Text className="nx-display" fz="xl">The Society</Text>}>
      <Stack>
        <TextInput label="Name" value={name} onChange={(e) => setName(e.currentTarget.value)} />
        <TextInput label="Headquarters" value={headquarters} onChange={(e) => setHeadquarters(e.currentTarget.value)} />
        <ErrorNote message={error} />
        <Group justify="flex-end">
          <Button variant="subtle" color="gold" onClick={onClose}>Cancel</Button>
          <Button onClick={save} loading={saving}>Save</Button>
        </Group>
      </Stack>
    </Modal>
  );
}
