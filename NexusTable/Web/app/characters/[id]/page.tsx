import { CharacterSheetPage } from "@/modules/characters/CharacterSheetPage";

export default async function CharacterPage({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;
  return <CharacterSheetPage characterId={id} />;
}
