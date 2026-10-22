// Logique de réservation partagée entre le front (React) et les fonctions serverless (/api).

export const SERVICES = {
  dejeuner: { label: 'Déjeuner', hours: '12:00 — 13:30', slots: ['12:00', '12:15', '12:30', '12:45', '13:00', '13:30'] },
  diner: { label: 'Dîner', hours: '19:00 — 21:30', slots: ['19:00', '19:30', '20:00', '20:30', '21:00', '21:30'] },
};

export const MIN_COVERS = 1;
export const MAX_COVERS = 8;
export const MAX_DAYS_AHEAD = 60;
export const CLOSED_WEEKDAYS = [0, 1]; // dimanche, lundi

export const toISODate = (d) =>
  `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;

export const fromISODate = (iso) => {
  const [y, m, d] = iso.split('-').map(Number);
  return new Date(y, m - 1, d);
};

const startOfDay = (d) => new Date(d.getFullYear(), d.getMonth(), d.getDate());

export function isDateBookable(date, today = new Date()) {
  const day = startOfDay(date);
  const t = startOfDay(today);
  const limit = new Date(t);
  limit.setDate(limit.getDate() + MAX_DAYS_AHEAD);
  return day >= t && day <= limit && !CLOSED_WEEKDAYS.includes(day.getDay());
}

// Hash déterministe : la même date/créneau donne toujours la même disponibilité,
// côté client comme côté serveur, sans base de données.
function hash(str) {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

export function getSlots(dateISO, serviceKey, covers = 2, now = new Date()) {
  const service = SERVICES[serviceKey];
  if (!service) return [];
  const date = fromISODate(dateISO);
  const isToday = toISODate(now) === dateISO;
  const pressure = covers >= 6 ? 5 : covers >= 4 ? 3 : 2;
  return service.slots.map((time) => {
    const [hh, mm] = time.split(':').map(Number);
    const slotDate = new Date(date.getFullYear(), date.getMonth(), date.getDate(), hh, mm);
    const past = isToday && slotDate.getTime() - now.getTime() < 2 * 60 * 60 * 1000;
    const full = hash(`${dateISO}|${serviceKey}|${time}`) % 10 < pressure;
    return { time, available: !past && !full };
  });
}

export function remainingTables(dateISO, now = new Date()) {
  const slots = getSlots(dateISO, 'diner', 2, now);
  return slots.filter((s) => s.available).length;
}

const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/;
const PHONE_RE = /^\+?[0-9 ().-]{8,20}$/;

export function validateReservation(r, now = new Date()) {
  const errors = {};
  if (!r.date || !/^\d{4}-\d{2}-\d{2}$/.test(r.date) || !isDateBookable(fromISODate(r.date), now)) {
    errors.date = 'Choisissez une date d’ouverture dans les 60 prochains jours.';
  }
  const covers = Number(r.covers);
  if (!Number.isInteger(covers) || covers < MIN_COVERS || covers > MAX_COVERS) {
    errors.covers = `Réservation en ligne de ${MIN_COVERS} à ${MAX_COVERS} couverts.`;
  }
  if (!SERVICES[r.service]) errors.service = 'Choisissez un service.';
  if (!errors.date && !errors.service) {
    const slot = getSlots(r.date, r.service, covers, now).find((s) => s.time === r.time);
    if (!slot) errors.time = 'Choisissez un créneau.';
    else if (!slot.available) errors.time = 'Ce créneau vient d’être réservé, merci d’en choisir un autre.';
  }
  if (!r.name || r.name.trim().length < 2) errors.name = 'Indiquez votre nom.';
  if (!r.email || !EMAIL_RE.test(r.email.trim())) errors.email = 'Adresse e-mail invalide.';
  if (!r.phone || !PHONE_RE.test(r.phone.trim())) errors.phone = 'Numéro de téléphone invalide.';
  if (r.notes && r.notes.length > 500) errors.notes = '500 caractères maximum.';
  return errors;
}

export function makeReference(r) {
  return `AUR-${(hash(`${r.date}${r.time}${r.email}${Date.now()}`) % 0xfffff).toString(36).toUpperCase().padStart(4, '0')}`;
}

export function formatLongDate(iso) {
  return fromISODate(iso).toLocaleDateString('fr-FR', { weekday: 'long', day: 'numeric', month: 'long' });
}
