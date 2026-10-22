import { getSlots, isDateBookable, fromISODate, SERVICES } from '../src/lib/booking.js';

// GET /api/availability?date=2026-11-14&service=diner&covers=2
export default function handler(req, res) {
  const { date, service = 'diner', covers = '2' } = req.query || {};
  if (!date || !/^\d{4}-\d{2}-\d{2}$/.test(date) || !SERVICES[service]) {
    return res.status(400).json({ error: 'Paramètres invalides.' });
  }
  if (!isDateBookable(fromISODate(date))) {
    return res.status(200).json({ date, service, slots: [], closed: true });
  }
  return res.status(200).json({ date, service, slots: getSlots(date, service, Number(covers)) });
}
