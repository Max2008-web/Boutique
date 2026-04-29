import Anthropic from '@anthropic-ai/sdk';

const SYSTEM_PROMPT = `Tu es un expert du bâtiment et un métreur professionnel en France.
À partir d'une description de travaux rédigée en langage courant par un artisan,
tu rédiges un devis professionnel, structuré, clair et détaillé, prêt à être collé
dans un logiciel de facturation.

Règles de rédaction :
- Utilise un ton professionnel et neutre, en français.
- Structure le devis par poste de travaux (Préparation, Dépose, Fournitures, Pose,
  Finitions, Nettoyage de chantier, etc.) selon ce qui s'applique.
- Pour chaque ligne, indique : libellé clair et technique, unité (m², ml, u, forfait, h),
  quantité estimée, prix unitaire HT indicatif (€), total HT (€).
- Inclus les fournitures principales et la main d'œuvre.
- Ajoute une section "Conditions" succincte (validité 30 jours, acompte 30%,
  délais à confirmer, TVA selon régime applicable).
- Termine par un récapitulatif : Total HT, TVA (10% rénovation logement >2 ans
  ou 20% selon le cas — précise l'hypothèse), Total TTC.
- Si la description est ambiguë, fais des hypothèses raisonnables et indique-les
  dans une courte section "Hypothèses".
- N'invente pas de coordonnées client/entreprise ; laisse des champs à compléter
  ([Nom du client], [Adresse du chantier], [Votre entreprise], [Date]).
- Réponds UNIQUEMENT avec le texte du devis, sans introduction ni commentaires.`;

export default async function handler(req, res) {
  if (req.method !== 'POST') {
    res.setHeader('Allow', 'POST');
    return res.status(405).json({ error: 'Méthode non autorisée' });
  }

  const apiKey = process.env.ANTHROPIC_API_KEY;
  if (!apiKey) {
    return res
      .status(500)
      .json({ error: 'ANTHROPIC_API_KEY non configurée sur le serveur.' });
  }

  const { description } = req.body || {};
  if (!description || typeof description !== 'string' || !description.trim()) {
    return res.status(400).json({ error: 'Description manquante.' });
  }

  try {
    const client = new Anthropic({ apiKey });
    const message = await client.messages.create({
      model: 'claude-sonnet-4-6',
      max_tokens: 2000,
      system: SYSTEM_PROMPT,
      messages: [
        {
          role: 'user',
          content: `Voici les travaux à devis :\n\n${description}`,
        },
      ],
    });

    const quote = message.content
      .filter((block) => block.type === 'text')
      .map((block) => block.text)
      .join('\n')
      .trim();

    return res.status(200).json({ quote });
  } catch (err) {
    console.error(err);
    return res
      .status(500)
      .json({ error: err.message || 'Erreur lors de la génération.' });
  }
}
