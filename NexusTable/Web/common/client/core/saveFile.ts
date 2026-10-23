/** Hands the browser a file to save, e.g. an export the API returned as text. */
export function saveFile(fileName: string, content: string, contentType: string) {
  const url = URL.createObjectURL(new Blob([content], { type: contentType }));
  const link = document.createElement("a");
  link.href = url;
  link.download = fileName;
  link.click();
  URL.revokeObjectURL(url);
}
