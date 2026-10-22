import { test } from 'node:test';
import assert from 'node:assert/strict';
import { getSlots, isDateBookable, validateReservation, fromISODate, toISODate } from '../src/lib/booking.js';

const NOW = new Date(2026, 9, 22, 10, 0); // jeudi 22 octobre 2026

test('fermé dimanche et lundi, ouvert du mardi au samedi', () => {
  assert.equal(isDateBookable(fromISODate('2026-10-25'), NOW), false); // dimanche
  assert.equal(isDateBookable(fromISODate('2026-10-26'), NOW), false); // lundi
  assert.equal(isDateBookable(fromISODate('2026-10-27'), NOW), true); // mardi
});

test('refuse le passé et au-delà de 60 jours', () => {
  assert.equal(isDateBookable(fromISODate('2026-10-21'), NOW), false);
  assert.equal(isDateBookable(fromISODate('2027-01-05'), NOW), false);
});

test('disponibilités déterministes', () => {
  assert.deepEqual(getSlots('2026-11-14', 'diner', 2, NOW), getSlots('2026-11-14', 'diner', 2, NOW));
  assert.equal(getSlots('2026-11-14', 'diner', 2, NOW).length, 6);
});

test('valide une réservation complète', () => {
  const slot = getSlots('2026-11-14', 'diner', 2, NOW).find((s) => s.available);
  const errors = validateReservation(
    { date: '2026-11-14', service: 'diner', time: slot.time, covers: 2, name: 'Camille Durand', email: 'camille@exemple.fr', phone: '+33 6 12 34 56 78' },
    NOW,
  );
  assert.deepEqual(errors, {});
});

test('signale les champs invalides', () => {
  const errors = validateReservation({ date: '2026-10-25', service: 'diner', time: '20:00', covers: 12, name: '', email: 'x', phone: '12' }, NOW);
  assert.ok(errors.date && errors.covers && errors.name && errors.email && errors.phone);
});

test('toISODate / fromISODate aller-retour', () => {
  assert.equal(toISODate(fromISODate('2026-11-03')), '2026-11-03');
});
