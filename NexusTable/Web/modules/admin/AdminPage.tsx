"use client";

import { Avatar, Badge, Box, Button, Group, Stack, Text } from "@mantine/core";
import { IconCheck, IconX } from "@tabler/icons-react";
import { useState } from "react";
import { type Account, AccountApi } from "@/common/api/account";
import { Http, useApi, useApiData } from "@/common/client/core/Api";
import { useMe } from "@/common/client/components/hooks";
import { ErrorNote, FullPageLoader, PageFrame, PageTitle } from "@/common/client/components/PageFrame";

/** Host only: who has signed in, and letting them in. New Discord sign-ins wait here. */
export function AdminPage() {
  return (
    <PageFrame>
      <AdminContent />
    </PageFrame>
  );
}

function AdminContent() {
  const { data: me } = useMe();
  const { data: accounts, error, mutate } = useApiData<Account[]>(me?.role === "Host" ? AccountApi.adminAccounts : null, { refreshInterval: 15000 });

  if (me && me.role !== "Host") return <Box maw={640} mx="auto"><ErrorNote message="Only the host manages accounts." /></Box>;
  if (error) return <Box maw={640} mx="auto"><ErrorNote message={error.message} /></Box>;
  if (!accounts) return <FullPageLoader label="Reading the guest book…" />;

  const waiting = accounts.filter((a) => !a.isApproved);
  const members = accounts.filter((a) => a.isApproved);
  return (
    <Box maw={720} mx="auto">
      <PageTitle kicker="Admin" title="Who may sit at the table">
        Everyone who signs in with Discord waits here until you let them in. Until then they see only that they are waiting.
      </PageTitle>
      <Stack gap="lg">
        <AccountList title={`Waiting · ${waiting.length}`} empty="Nobody is waiting." accounts={waiting} onChanged={() => void mutate()} />
        <AccountList title={`Let in · ${members.length}`} empty="Nobody yet." accounts={members} onChanged={() => void mutate()} />
      </Stack>
    </Box>
  );
}

function AccountList({ title, empty, accounts, onChanged }: { title: string; empty: string; accounts: Account[]; onChanged: () => void }) {
  return (
    <Box className="nx-panel" p="md">
      <Text className="nx-kicker" mb="xs">{title}</Text>
      {accounts.length === 0 ? (
        <Text fz="sm" c="var(--nx-sepia)">{empty}</Text>
      ) : (
        <Stack gap={8}>
          {accounts.map((a) => <AccountRow key={a.id} account={a} onChanged={onChanged} />)}
        </Stack>
      )}
    </Box>
  );
}

function AccountRow({ account, onChanged }: { account: Account; onChanged: () => void }) {
  const call = useApi();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string>();

  const set = async (approved: boolean) => {
    setBusy(true);
    const result = await call<Account>(Http.PUT, AccountApi.approval(account.id), { approved });
    setBusy(false);
    setError(result.success ? undefined : result.errorMessage);
    if (result.success) onChanged();
  };

  return (
    <Box>
      <Group justify="space-between" wrap="nowrap">
        <Group gap="sm" wrap="nowrap" style={{ minWidth: 0 }}>
          <Avatar src={account.avatarUrl} radius="xl" color="gold">{account.displayName[0]}</Avatar>
          <Box style={{ minWidth: 0 }}>
            <Group gap={6}>
              <Text className="nx-display" fz="md" truncate>{account.displayName}</Text>
              {account.role === "Host" && <Badge size="xs" color="blood">Host</Badge>}
            </Group>
            <Text fz={11} c="var(--nx-sepia)" truncate>
              Signed in first {new Date(account.createdAt).toLocaleDateString()} · {account.subject.startsWith("oauth2|discord") ? "Discord" : account.subject}
            </Text>
          </Box>
        </Group>
        {account.role !== "Host" && (account.isApproved ? (
          <Button size="compact-sm" variant="outline" color="blood.4" leftSection={<IconX size={14} />} loading={busy} onClick={() => void set(false)}>Turn away</Button>
        ) : (
          <Button size="compact-sm" color="gold" leftSection={<IconCheck size={14} />} loading={busy} onClick={() => void set(true)}>Let in</Button>
        ))}
      </Group>
      <ErrorNote message={error} />
    </Box>
  );
}
