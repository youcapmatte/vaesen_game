"use client";

import { Anchor, Avatar, Badge, Box, Button, Center, Group, Loader, Menu, Stack, Text, Title, UnstyledButton } from "@mantine/core";
import { IconBuildingCastle, IconCandle, IconCards, IconChevronDown, IconLogout, IconScript, IconUserPlus, IconUsers } from "@tabler/icons-react";
import Link from "next/link";
import { usePathname, useRouter } from "next/navigation";
import { useEffect } from "react";
import { useSecurity } from "../security/SecurityProvider";
import { useTableStatus } from "../signalr/useTable";
import { useMe } from "./hooks";
import { Divider, SightMark } from "./Ornament";

const NAV = [
  { href: "/society", label: "The Society", icon: IconBuildingCastle },
  { href: "/table", label: "The Table", icon: IconCandle },
  { href: "/mysteries", label: "Case Files", icon: IconScript, hostOnly: true },
  { href: "/world", label: "World", icon: IconUsers, hostOnly: true },
];

/** The frame of every signed-in page: header with navigation and the player menu. Sends signed-out visitors home. */
export function PageFrame({ children, fullBleed }: { children: React.ReactNode; fullBleed?: boolean }) {
  const { status, signOut } = useSecurity();
  const router = useRouter();
  const pathname = usePathname();
  const { data: me } = useMe();
  const live = useTableStatus();
  const nav = NAV.filter((item) => !("hostOnly" in item) || me?.role === "Host");

  useEffect(() => {
    if (status === "signedOut") {
      router.replace("/");
    }
  }, [status, router]);

  if (status !== "signedIn") {
    return <FullPageLoader />;
  }

  return (
    <Box mih="100vh" style={{ display: "flex", flexDirection: "column" }}>
      <Box
        component="header"
        px="lg"
        py={10}
        style={{ borderBottom: "1px solid var(--nx-gold-dim)", background: "rgba(20,17,14,0.85)", backdropFilter: "blur(4px)", position: "sticky", top: 0, zIndex: 50 }}
      >
        <Group justify="space-between" wrap="nowrap">
          <Group gap="xl" wrap="nowrap">
            <Anchor component={Link} href="/society" underline="never">
              <Group gap={8} wrap="nowrap">
                <SightMark size={18} />
                <Text className="nx-display" fz={22} c="var(--nx-parchment)">Nexus Table</Text>
              </Group>
            </Anchor>
            <Group gap={4} visibleFrom="sm">
              {nav.map(({ href, label, icon: Icon }) => (
                <Button
                  key={href}
                  component={Link}
                  href={href}
                  variant={pathname.startsWith(href) ? "light" : "subtle"}
                  color={pathname.startsWith(href) ? "gold" : "umber.2"}
                  leftSection={<Icon size={16} stroke={1.5} />}
                  size="compact-md"
                >
                  {label}
                </Button>
              ))}
            </Group>
          </Group>

          <Group gap="sm" wrap="nowrap">
            <Badge variant="dot" color={live === "online" ? "moss.3" : live === "connecting" ? "gold" : "blood.5"} visibleFrom="xs">
              {live === "online" ? "At the table" : live === "connecting" ? "Lighting candles" : "Away"}
            </Badge>
            <Menu position="bottom-end" width={220}>
              <Menu.Target>
                <UnstyledButton>
                  <Group gap={8} wrap="nowrap">
                    <Avatar src={me?.avatarUrl} radius="xl" size={30} color="gold">
                      {me?.displayName?.[0]}
                    </Avatar>
                    <Text visibleFrom="sm" fz="sm">{me?.displayName ?? "…"}</Text>
                    {me?.role === "Host" && <Badge size="xs" color="blood" visibleFrom="sm">Host</Badge>}
                    <IconChevronDown size={14} />
                  </Group>
                </UnstyledButton>
              </Menu.Target>
              <Menu.Dropdown>
                {nav.map(({ href, label, icon: Icon }) => (
                  <Menu.Item key={href} hiddenFrom="sm" component={Link} href={href} leftSection={<Icon size={16} />}>{label}</Menu.Item>
                ))}
                {me?.activeCharacterId ? (
                  <Menu.Item component={Link} href={`/characters/${me.activeCharacterId}`} leftSection={<IconCards size={16} />}>My character</Menu.Item>
                ) : (
                  <Menu.Item component={Link} href="/characters/new" leftSection={<IconUserPlus size={16} />}>Create a character</Menu.Item>
                )}
                <Menu.Divider />
                <Menu.Item color="blood.4" leftSection={<IconLogout size={16} />} onClick={signOut}>Leave the table</Menu.Item>
              </Menu.Dropdown>
            </Menu>
          </Group>
        </Group>
      </Box>

      <Box component="main" style={{ flex: 1, minHeight: 0 }} px={fullBleed ? 0 : "md"} py={fullBleed ? 0 : "xl"}>
        {children}
      </Box>
    </Box>
  );
}

export function FullPageLoader({ label = "Lighting the candles…" }: { label?: string }) {
  return (
    <Center mih="100vh">
      <Stack align="center" gap="sm">
        <Loader color="gold" type="dots" />
        <Text className="nx-flourish" c="var(--nx-sepia)">{label}</Text>
      </Stack>
    </Center>
  );
}

/** A page title in the book's manner: kicker, display title and an ornament rule. */
export function PageTitle({ kicker, title, children }: { kicker?: string; title: string; children?: React.ReactNode }) {
  return (
    <Stack gap={6} align="center" mb="xl" ta="center">
      {kicker && <Text className="nx-kicker">{kicker}</Text>}
      <Title order={1}>{title}</Title>
      <Divider width={220} />
      {children && <Text className="nx-flourish" c="var(--nx-parchment-2)" maw={620} mt={4}>{children}</Text>}
    </Stack>
  );
}

export function ErrorNote({ message }: { message?: string }) {
  if (!message) {
    return null;
  }
  return (
    <Text c="blood.3" fz="sm" className="nx-flourish">
      {message}
    </Text>
  );
}
