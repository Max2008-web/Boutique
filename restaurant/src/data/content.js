export const RESTAURANT = {
  name: 'Maison Aurèle',
  chef: 'Aurèle Vasseur',
  phone: '+33 1 42 00 00 00',
  phoneHref: 'tel:+33142000000',
  email: 'bonjour@maison-aurele.fr',
  instagram: '@maisonaurele',
  address: ['12 rue de Miromesnil', '75008 Paris'],
  access: 'Métro Miromesnil (L9, L13) · Voiturier le soir',
  mapsUrl: 'https://maps.google.com/?q=12+rue+de+Miromesnil+75008+Paris',
};

export const HOURS = [
  { label: 'Déjeuner', value: 'Mar — Sam · 12:00 — 13:30' },
  { label: 'Dîner', value: 'Mar — Sam · 19:00 — 21:30' },
  { label: 'Fermeture', value: 'Dimanche & lundi' },
];

export const MENU = {
  entrees: {
    label: 'Entrées',
    items: [
      { name: 'Langoustine de Bretagne', price: '48', desc: 'Crue et juste saisie, beurre noisette, agrumes confits, caviar osciètre' },
      { name: 'Cèpes du Limousin', price: '42', desc: 'Rôtis au sautoir, jaune d’œuf confit, bouillon de foin, noisettes torréfiées' },
      { name: 'Saint-Jacques de Dieppe', price: '46', desc: 'En carpaccio, betterave crapaudine, raifort frais, huile de livèche' },
      { name: 'Foie gras de canard', price: '44', desc: 'Cuit au torchon, figue noire rôtie, brioche feuilletée au poivre de Timut' },
    ],
  },
  plats: {
    label: 'Plats',
    items: [
      { name: 'Turbot sauvage à la braise', price: '78', desc: 'Beurre blanc au vin jaune, poireaux crayon, coquillages de saison' },
      { name: 'Pigeon de Racan', price: '72', desc: 'Rôti sur coffre, betterave fumée, jus corsé au cacao, baies de genièvre' },
      { name: 'Ris de veau du Limousin', price: '68', desc: 'Croustillant, truffe noire, céleri-rave en croûte de sel' },
      { name: 'Homard bleu', price: '84', desc: 'Grillé au sarment, bisque montée à l’estragon, gnocchi de potimarron' },
    ],
  },
  desserts: {
    label: 'Desserts',
    items: [
      { name: 'Chocolat Grand Cru & or', price: '26', desc: 'Dôme Guanaja 70 %, praliné noisette, crème glacée au lait fumé' },
      { name: 'Poire Williams', price: '24', desc: 'Pochée au poiré, sablé breton, sorbet au sapin de Douglas' },
      { name: 'Soufflé chaud', price: '28', desc: 'Au Grand Marnier, glace vanille de Tahiti — 20 minutes' },
      { name: 'Plateau de fromages affinés', price: '22', desc: 'Sélection de Maison Quatrehomme, confiture de cerises noires' },
    ],
  },
  vins: {
    label: 'Carte des vins',
    items: [
      { name: 'Champagne Jacques Selosse, Initial', price: '38 / 260', desc: 'Blanc de blancs · Avize · verre / bouteille' },
      { name: 'Meursault 1er Cru « Perrières » 2019', price: '— / 340', desc: 'Domaine Roulot · Bourgogne blanc' },
      { name: 'Côte-Rôtie « La Landonne » 2017', price: '— / 420', desc: 'E. Guigal · Rhône septentrional' },
      { name: 'Château Rieussec 2009', price: '24 / 180', desc: 'Sauternes · liquoreux · verre / bouteille' },
    ],
  },
};

export const TASTING_MENU = {
  title: 'Dégustation en sept temps',
  price: '185',
  pairing: '120',
  steps: [
    'Amuse-bouche du moment',
    'Langoustine, caviar & agrumes',
    'Cèpes, foin & noisette',
    'Turbot sauvage à la braise',
    'Pigeon de Racan, betterave',
    'Pré-dessert',
    'Chocolat Grand Cru & or',
  ],
};

export const GALLERY = [
  { key: 'salle', caption: 'La salle · 32 couverts', className: 'md:col-span-2 md:row-span-2' },
  { key: 'plat', caption: 'Pigeon de Racan, betterave', className: 'md:col-span-2' },
  { key: 'cave', caption: 'La cave · 640 références', className: 'md:row-span-2' },
  { key: 'hero', caption: 'Langoustine & caviar', className: 'md:row-span-2' },
  { key: 'dessert', caption: 'Chocolat Grand Cru & or', className: '' },
  { quote: '« Une expérience d’une rare élégance. »', source: 'Le Guide des Gourmets', className: '' },
];

export const PHOTO_ALT = {
  hero: 'Langoustine dressée sur assiette noire, lumière tamisée',
  chef: 'Le Chef dressant une assiette à la pince en cuisine',
  salle: 'Salle du restaurant, nappes crème et suspensions en laiton',
  plat: 'Pigeon rôti, betterave et herbes sauvages sur assiette crème',
  cave: 'Le sommelier servant un vin rouge dans la cave voûtée',
  dessert: 'Dôme de chocolat à la feuille d’or et quenelle de crème',
};
