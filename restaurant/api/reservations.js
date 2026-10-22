import { validateReservation, makeReference, SERVICES } from '../src/lib/booking.js';

// POST /api/reservations — valide la demande et renvoie une confirmation.
// Branchez ici votre stockage (base de données, Google Sheets, outil de réservation…)
// et l'envoi de l'e-mail de confirmation.
export default async function handler(req, res) {
  if (req.method !== 'POST') {
    res.setHeader('Allow', 'POST');
    return res.status(405).json({ error: 'Méthode non autorisée.' });
  }
  const body = typeof req.body === 'string' ? JSON.parse(req.body || '{}') : req.body || {};
  const reservation = {
    date: body.date,
    service: body.service,
    time: body.time,
    covers: Number(body.covers),
    name: String(body.name || '').trim(),
    email: String(body.email || '').trim(),
    phone: String(body.phone || '').trim(),
    occasion: String(body.occasion || '').trim(),
    notes: String(body.notes || '').trim(),
  };
  const errors = validateReservation(reservation);
  if (Object.keys(errors).length) return res.status(422).json({ errors });

  const reference = makeReference(reservation);
  return res.status(201).json({
    reference,
    status: 'confirmed',
    reservation: { ...reservation, serviceLabel: SERVICES[reservation.service].label },
  });
}
