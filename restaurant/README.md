# Maison Aurèle — site de restaurant gastronomique

Site vitrine interactif d'un restaurant gastronomique : hero plein écran, concept & Chef,
carte à onglets, galerie bento avec visionneuse, module de réservation et footer.

- **Design** : fichier Figma « Restaurant - UI/UX » — https://www.figma.com/design/6co9gnQRpNDhqStqTKupBq
  - `00 · Foundations` : palette, typographie, grille 8pt, banque photo
  - `01 · Components` : Button, Logo, SectionHeader, MenuItem, MenuTab, TimeSlot, CalendarDay, Field
  - `02 · Desktop 1440` et `03 · Mobile 390`
- **Stack** : React 18 + Vite + Tailwind CSS, fonctions serverless Vercel (`/api`).

## Lancer en local

```bash
cd restaurant
npm install
npm run images   # récupère les photos HD (voir « Photos » ci-dessous)
npm run dev      # http://localhost:5173
npm test         # tests de la logique de réservation
```

`npm run dev` (Vite seul) ne sert pas `/api` : le formulaire bascule alors sur la
validation locale. Pour tester l'API, lancez `vercel dev` depuis `restaurant/`.

## Structure

```
restaurant/
├── api/
│   ├── availability.js   # GET  /api/availability?date=YYYY-MM-DD&service=diner&covers=2
│   └── reservations.js   # POST /api/reservations (validation + référence de confirmation)
├── public/images/        # photos HD (+ lqip/ : aperçus basse définition)
├── src/
│   ├── components/       # Nav, Hero, Concept, Menu, Gallery, Reservation, Footer, StickyCta, ui
│   ├── data/content.js   # textes, carte, horaires, coordonnées
│   └── lib/booking.js    # règles de réservation partagées front / API
├── test/booking.test.js
└── tailwind.config.js    # tokens miroirs des variables Figma
```

## Règles de réservation (`src/lib/booking.js`)

- Ouvert du mardi au samedi, réservation jusqu'à 60 jours à l'avance.
- 1 à 8 couverts en ligne (au-delà : téléphone).
- Déjeuner 12:00–13:30, dîner 19:00–21:30. Créneaux du jour fermés à moins de 2 h.
- La disponibilité est simulée de façon déterministe. Pour la production, remplacez
  `getSlots` par une lecture de votre base ou de votre outil de réservation, et
  enregistrez la réservation (et envoyez l'e-mail) dans `api/reservations.js`.

## Photos

Les visuels ont été générés avec Figma AI dans le fichier de design (calques `photo/*`
de la page Foundations). `npm run images` les télécharge dans `public/images/` ; les
liens expirent **7 jours** après la génération. Ensuite, exportez les calques en JPG
depuis Figma sous les noms `hero`, `chef`, `salle`, `plat`, `cave` et `dessert`.
Sans les fichiers HD, le site affiche les aperçus de `public/images/lqip/`.

## Déploiement

Sur Vercel, créez un projet avec **Root Directory = `restaurant`**. Vercel détecte Vite
et publie automatiquement les fonctions de `api/`.
