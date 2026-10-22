import { CaseFilePage } from "@/modules/mysteries/CaseFilePage";

export default async function MysteryPage({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;
  return <CaseFilePage mysteryId={id} />;
}
