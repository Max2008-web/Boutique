# Générateur de devis pour artisans

Petite application React + Tailwind. L'artisan tape une description des travaux
en langage courant, l'application génère un devis professionnel via l'API Claude,
et un bouton **Copier** permet de le coller dans un logiciel de facturation.

## Structure

```
.
├── api/generate.js        # Fonction serverless (appelle l'API Claude)
├── src/
│   ├── App.jsx            # Interface (champ + bouton Générer + bouton Copier)
│   ├── main.jsx
│   └── index.css          # Tailwind
├── index.html
├── package.json
├── tailwind.config.js
├── postcss.config.js
└── vite.config.js
```

## Lancer en local

Pré-requis : **Node.js 18+** installé.

```bash
# 1. Installer les dépendances
npm install

# 2. Renseigner la clé API Claude
cp .env.example .env
# puis éditez .env et collez votre clé ANTHROPIC_API_KEY

# 3. Démarrer (avec Vercel CLI pour la fonction /api)
npm install -g vercel
vercel dev
```

Ouvrez http://localhost:3000.

> Astuce : `npm run dev` (Vite seul) sert le frontend mais ne fait pas tourner la
> fonction `/api/generate`. Pour tester l'app complète en local, utilisez `vercel dev`.

## Déployer sur Vercel (recommandé, gratuit)

1. Créez un compte sur https://vercel.com (connexion GitHub conseillée).
2. Poussez ce dossier sur un dépôt GitHub.
3. Sur Vercel : **New Project** → importez le dépôt.
4. Dans **Settings → Environment Variables**, ajoutez :
   - `ANTHROPIC_API_KEY` = votre clé (à récupérer sur https://console.anthropic.com)
5. **Deploy**. Vercel détecte Vite automatiquement et publie l'API serverless.

## Déployer sur Netlify

Identique à Vercel, mais déplacez `api/generate.js` dans `netlify/functions/generate.js`
et adaptez la signature (handler + event/context). Variable d'environnement
`ANTHROPIC_API_KEY` à définir dans le dashboard Netlify.

## Sécurité

La clé API n'est **jamais** exposée au navigateur : elle vit uniquement côté
serveur dans la fonction `api/generate.js`. Ne committez jamais votre `.env`.

---

## Autre projet : site du restaurant « Maison Aurèle »

Le dossier [`restaurant/`](restaurant/README.md) contient un site de restaurant gastronomique
(React + Vite + Tailwind, réservation en ligne) et le lien vers sa maquette Figma.
