// Télécharge les photos HD générées dans le fichier Figma « Restaurant - UI/UX »
// vers public/images/. Les liens Figma expirent 7 jours après la génération ;
// passé ce délai, exportez les calques « photo/* » de la page Foundations en JPG.
import { writeFile, mkdir } from 'node:fs/promises';

const ASSETS = {
  hero: 'fb2a7ebc-bafd-474a-ab1b-9a5adfe1577a',
  chef: '28f00d99-b18b-446c-91c8-0306dddaf688',
  salle: '17b2592a-a1a3-4556-a1de-8ed89e7ee606',
  dessert: 'ee8d1bfc-26ed-472a-a722-be5c2388a599',
  cave: 'eb16cdf0-cb46-4a9b-a3b4-01df0cc0a141',
  plat: 'f70d13e5-cc09-4707-97f2-7d02892f02e0',
};

const dir = new URL('../public/images/', import.meta.url);
await mkdir(dir, { recursive: true });
for (const [name, id] of Object.entries(ASSETS)) {
  const res = await fetch(`https://www.figma.com/api/mcp/asset/${id}.jpg`);
  if (!res.ok) {
    console.error(`✗ ${name} (${res.status}) — exportez « photo/${name} » depuis Figma vers public/images/${name}.jpg`);
    continue;
  }
  await writeFile(new URL(`${name}.jpg`, dir), Buffer.from(await res.arrayBuffer()));
  console.log(`✓ public/images/${name}.jpg`);
}
