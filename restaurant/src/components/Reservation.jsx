import { useEffect, useMemo, useState } from 'react';
import { ArrowIcon, ChevronIcon, CloseIcon, Reveal, SectionHeader } from './ui.jsx';
import { HOURS, RESTAURANT } from '../data/content.js';
import {
  MAX_COVERS,
  MAX_DAYS_AHEAD,
  MIN_COVERS,
  SERVICES,
  formatLongDate,
  fromISODate,
  getSlots,
  isDateBookable,
  makeReference,
  toISODate,
  validateReservation,
} from '../lib/booking.js';

const WEEKDAYS = ['L', 'M', 'M', 'J', 'V', 'S', 'D'];
const STEPS = ['Date', 'Couverts', 'Créneau', 'Coordonnées'];

function firstBookableDate(today) {
  const d = new Date(today);
  for (let i = 0; i <= MAX_DAYS_AHEAD; i++) {
    if (isDateBookable(d, today)) return toISODate(d);
    d.setDate(d.getDate() + 1);
  }
  return null;
}

function monthMatrix(year, month) {
  const first = new Date(year, month, 1);
  const offset = (first.getDay() + 6) % 7; // semaine commençant le lundi
  const days = new Date(year, month + 1, 0).getDate();
  const cells = Array.from({ length: offset }, () => null);
  for (let d = 1; d <= days; d++) cells.push(new Date(year, month, d));
  while (cells.length % 7) cells.push(null);
  return cells;
}

async function submitReservation(payload) {
  try {
    const res = await fetch('/api/reservations', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    // En développement (Vite seul), /api n'existe pas : on bascule sur la validation locale.
    if (res.status === 404 || !res.headers.get('content-type')?.includes('json')) throw new Error('offline');
    const data = await res.json();
    if (!res.ok) return { errors: data.errors || { form: data.error || 'Une erreur est survenue.' } };
    return data;
  } catch {
    const errors = validateReservation(payload);
    if (Object.keys(errors).length) return { errors };
    await new Promise((r) => setTimeout(r, 700));
    return { reference: makeReference(payload), status: 'confirmed', reservation: payload };
  }
}

export default function Reservation() {
  const today = useMemo(() => new Date(), []);
  const [date, setDate] = useState(() => firstBookableDate(today));
  const [cursor, setCursor] = useState(() => {
    const d = date ? fromISODate(date) : today;
    return { y: d.getFullYear(), m: d.getMonth() };
  });
  const [covers, setCovers] = useState(2);
  const [service, setService] = useState('diner');
  const [time, setTime] = useState(null);
  const [form, setForm] = useState({ name: '', email: '', phone: '', occasion: '', notes: '' });
  const [errors, setErrors] = useState({});
  const [status, setStatus] = useState('idle');
  const [confirmation, setConfirmation] = useState(null);

  const slots = useMemo(() => (date ? getSlots(date, service, covers, today) : []), [date, service, covers, today]);

  // Le créneau choisi peut devenir indisponible quand on change de date, de service ou de couverts.
  useEffect(() => {
    if (time && !slots.find((s) => s.time === time && s.available)) setTime(null);
  }, [slots, time]);

  const cells = useMemo(() => monthMatrix(cursor.y, cursor.m), [cursor]);
  const monthLabel = new Date(cursor.y, cursor.m, 1).toLocaleDateString('fr-FR', { month: 'long', year: 'numeric' });
  const minMonth = today.getFullYear() * 12 + today.getMonth();
  const limit = new Date(today);
  limit.setDate(limit.getDate() + MAX_DAYS_AHEAD);
  const maxMonth = limit.getFullYear() * 12 + limit.getMonth();
  const curMonth = cursor.y * 12 + cursor.m;
  const moveMonth = (d) => {
    const next = new Date(cursor.y, cursor.m + d, 1);
    setCursor({ y: next.getFullYear(), m: next.getMonth() });
  };

  const formFilled = form.name && form.email && form.phone;
  const stepIndex = !date ? 0 : !time ? 2 : !formFilled ? 3 : 4;

  const update = (k) => (e) => {
    setForm((f) => ({ ...f, [k]: e.target.value }));
    if (errors[k]) setErrors((er) => ({ ...er, [k]: undefined }));
  };

  const onSubmit = async (e) => {
    e.preventDefault();
    const payload = { date, service, time, covers, ...form };
    const localErrors = validateReservation(payload, today);
    if (Object.keys(localErrors).length) {
      setErrors(localErrors);
      const first = ['date', 'time', 'name', 'email', 'phone'].find((k) => localErrors[k]);
      document.getElementById(`resa-${first}`)?.focus();
      return;
    }
    setStatus('loading');
    const result = await submitReservation(payload);
    if (result.errors) {
      setErrors(result.errors);
      setStatus('idle');
      return;
    }
    setConfirmation(result);
    setStatus('done');
  };

  const reset = () => {
    setConfirmation(null);
    setStatus('idle');
    setTime(null);
    setForm({ name: '', email: '', phone: '', occasion: '', notes: '' });
  };

  return (
    <section id="reservation" aria-labelledby="resa-title" className="border-y border-line-subtle bg-bg-base py-24 md:py-40">
      <div className="container-lux grid gap-16 lg:grid-cols-[400px_1fr] lg:gap-20">
        <Reveal className="flex flex-col gap-10">
          <SectionHeader id="resa-title" overline="Réservation" title="Réservez votre table" />
          <p className="text-ink-secondary">
            Réservations ouvertes jusqu’à {MAX_DAYS_AHEAD} jours à l’avance. Pour les groupes de plus de {MAX_COVERS} personnes ou une
            privatisation, contactez directement notre maître d’hôtel.
          </p>
          <dl>
            {HOURS.map((h) => (
              <div key={h.label} className="flex items-center justify-between border-b border-line-subtle py-4">
                <dt className="font-serif text-2xl font-medium">{h.label}</dt>
                <dd className="text-sm text-ink-secondary">{h.value}</dd>
              </div>
            ))}
          </dl>
          <p className="text-sm text-ink-muted">
            Annulation gratuite jusqu’à 48 h avant le service. Une empreinte bancaire est demandée pour le menu dégustation.
          </p>
          <a href={RESTAURANT.phoneHref} className="btn-ghost self-start">
            {RESTAURANT.phone}
          </a>
        </Reveal>

        <Reveal as="form" delay={100} onSubmit={onSubmit} noValidate className="flex flex-col gap-10 border border-line-subtle bg-bg-elevated p-6 md:p-12">
          {/* Étapes */}
          <ol className="flex flex-wrap items-center gap-x-4 gap-y-3" aria-label="Progression">
            {STEPS.map((s, i) => {
              const done = i < stepIndex;
              return (
                <li key={s} className="flex items-center gap-3">
                  <span
                    className={`flex h-7 w-7 items-center justify-center rounded-full border text-xs transition-colors ${
                      done ? 'border-brass-500 bg-brass-500 text-ink-inverse' : 'border-line-brass text-ink-muted'
                    }`}
                    aria-hidden="true"
                  >
                    {i + 1}
                  </span>
                  <span className={`eyebrow text-[11px] ${done ? 'text-ink-primary' : 'text-ink-muted'}`}>
                    {s}
                    <span className="sr-only">{done ? ' — complété' : ''}</span>
                  </span>
                  {i < STEPS.length - 1 && <span className="hidden h-px w-6 bg-line-brass sm:block" aria-hidden="true" />}
                </li>
              );
            })}
          </ol>

          <div className="grid gap-10 xl:grid-cols-[360px_1fr] xl:gap-12">
            {/* Calendrier */}
            <fieldset className="flex flex-col gap-4">
              <legend className="sr-only">Date</legend>
              <div className="flex items-center justify-between">
                <button type="button" onClick={() => moveMonth(-1)} disabled={curMonth <= minMonth} className="p-2 text-brass-500 disabled:opacity-30" aria-label="Mois précédent">
                  <ChevronIcon dir="left" />
                </button>
                <span className="font-serif text-3xl capitalize" aria-live="polite">
                  {monthLabel}
                </span>
                <button type="button" onClick={() => moveMonth(1)} disabled={curMonth >= maxMonth} className="p-2 text-brass-500 disabled:opacity-30" aria-label="Mois suivant">
                  <ChevronIcon />
                </button>
              </div>
              <div className="grid grid-cols-7 gap-1 text-center" id="resa-date" tabIndex={-1}>
                {WEEKDAYS.map((d, i) => (
                  <span key={i} className="pb-2 text-[11px] font-semibold tracking-[0.2em] text-ink-muted" aria-hidden="true">
                    {d}
                  </span>
                ))}
                {cells.map((d, i) => {
                  if (!d) return <span key={i} />;
                  const iso = toISODate(d);
                  const bookable = isDateBookable(d, today);
                  const selected = iso === date;
                  const isToday = iso === toISODate(today);
                  return (
                    <button
                      key={i}
                      type="button"
                      disabled={!bookable}
                      onClick={() => {
                        setDate(iso);
                        setErrors((er) => ({ ...er, date: undefined }));
                      }}
                      aria-pressed={selected}
                      aria-label={d.toLocaleDateString('fr-FR', { weekday: 'long', day: 'numeric', month: 'long' }) + (bookable ? '' : ' — fermé')}
                      className={`mx-auto flex h-11 w-11 items-center justify-center rounded-full text-[15px] transition-all duration-200 ${
                        selected
                          ? 'bg-brass-500 text-ink-inverse'
                          : bookable
                            ? `text-ink-primary hover:bg-bg-overlay ${isToday ? 'border border-line-brass' : ''}`
                            : 'text-ink-muted/50'
                      }`}
                    >
                      {d.getDate()}
                    </button>
                  );
                })}
              </div>
              {errors.date && <p className="text-sm text-red-300">{errors.date}</p>}
            </fieldset>

            <div className="flex flex-col gap-8">
              {/* Couverts */}
              <div className="flex flex-col gap-2">
                <span className="eyebrow text-ink-muted" id="covers-label">
                  Couverts
                </span>
                <div className="flex items-center justify-between border-b border-line-brass py-3" role="group" aria-labelledby="covers-label">
                  <button
                    type="button"
                    onClick={() => setCovers((c) => Math.max(MIN_COVERS, c - 1))}
                    disabled={covers <= MIN_COVERS}
                    className="flex h-10 w-10 items-center justify-center rounded-full border border-line-brass text-brass-500 transition-colors hover:border-brass-500 disabled:opacity-30"
                    aria-label="Retirer un couvert"
                  >
                    −
                  </button>
                  <output className="font-serif text-3xl" aria-live="polite">
                    {covers} {covers > 1 ? 'personnes' : 'personne'}
                  </output>
                  <button
                    type="button"
                    onClick={() => setCovers((c) => Math.min(MAX_COVERS, c + 1))}
                    disabled={covers >= MAX_COVERS}
                    className="flex h-10 w-10 items-center justify-center rounded-full border border-line-brass text-brass-500 transition-colors hover:border-brass-500 disabled:opacity-30"
                    aria-label="Ajouter un couvert"
                  >
                    +
                  </button>
                </div>
                {covers >= MAX_COVERS && <p className="text-xs text-ink-muted">Au-delà de {MAX_COVERS} convives, appelez-nous au {RESTAURANT.phone}.</p>}
              </div>

              {/* Service */}
              <div className="flex flex-col gap-3">
                <span className="eyebrow text-ink-muted">Service</span>
                <div className="grid grid-cols-2 border border-line-brass" role="radiogroup" aria-label="Service">
                  {Object.entries(SERVICES).map(([key, s]) => (
                    <button
                      key={key}
                      type="button"
                      role="radio"
                      aria-checked={service === key}
                      onClick={() => setService(key)}
                      className={`py-3 text-btn font-semibold uppercase transition-colors ${service === key ? 'bg-brass-500 text-ink-inverse' : 'text-ink-secondary hover:text-ink-primary'}`}
                    >
                      {s.label}
                    </button>
                  ))}
                </div>
              </div>

              {/* Créneaux */}
              <div className="flex flex-col gap-3">
                <span className="eyebrow text-ink-muted">Créneaux disponibles</span>
                <div className="grid grid-cols-3 gap-2" role="radiogroup" aria-label="Créneau horaire" id="resa-time" tabIndex={-1}>
                  {slots.map((s) => (
                    <button
                      key={s.time}
                      type="button"
                      role="radio"
                      aria-checked={time === s.time}
                      disabled={!s.available}
                      onClick={() => {
                        setTime(s.time);
                        setErrors((er) => ({ ...er, time: undefined }));
                      }}
                      className={`border py-3 text-sm transition-all duration-200 ${
                        time === s.time
                          ? 'border-brass-500 bg-brass-500 text-ink-inverse'
                          : s.available
                            ? 'border-line-subtle text-ink-primary hover:border-brass-500'
                            : 'border-line-subtle text-ink-muted line-through opacity-50'
                      }`}
                    >
                      {s.time}
                      {!s.available && <span className="sr-only"> — complet</span>}
                    </button>
                  ))}
                </div>
                {slots.length > 0 && slots.every((s) => !s.available) && <p className="text-sm text-ink-muted">Complet pour ce service — essayez un autre jour.</p>}
                {errors.time && <p className="text-sm text-red-300">{errors.time}</p>}
              </div>
            </div>
          </div>

          <div className="h-px bg-line-subtle" />

          {/* Coordonnées */}
          <div className="grid gap-8 md:grid-cols-3">
            {[
              ['name', 'Nom', 'text', 'name', 'Camille Durand'],
              ['email', 'E-mail', 'email', 'email', 'camille@exemple.fr'],
              ['phone', 'Téléphone', 'tel', 'tel', '+33 6 12 34 56 78'],
            ].map(([k, label, type, auto, ph]) => (
              <label key={k} className="flex flex-col gap-2">
                <span className="eyebrow text-ink-muted">{label}</span>
                <input
                  id={`resa-${k}`}
                  type={type}
                  autoComplete={auto}
                  placeholder={ph}
                  value={form[k]}
                  onChange={update(k)}
                  aria-invalid={!!errors[k]}
                  aria-describedby={errors[k] ? `err-${k}` : undefined}
                  className={`field-input text-xl ${errors[k] ? 'border-red-300' : ''}`}
                />
                {errors[k] && (
                  <span id={`err-${k}`} className="text-sm text-red-300">
                    {errors[k]}
                  </span>
                )}
              </label>
            ))}
          </div>
          <div className="grid gap-8 md:grid-cols-[1fr_2fr]">
            <label className="flex flex-col gap-2">
              <span className="eyebrow text-ink-muted">Occasion (facultatif)</span>
              <select value={form.occasion} onChange={update('occasion')} className="field-input cursor-pointer text-xl [&>option]:bg-bg-elevated">
                <option value="">—</option>
                <option>Anniversaire</option>
                <option>Dîner d’affaires</option>
                <option>Demande en mariage</option>
                <option>Célébration</option>
              </select>
            </label>
            <label className="flex flex-col gap-2">
              <span className="eyebrow text-ink-muted">Allergies, demandes particulières</span>
              <input type="text" value={form.notes} onChange={update('notes')} maxLength={500} placeholder="Sans gluten, table calme…" className="field-input text-xl" />
            </label>
          </div>

          <div className="flex flex-col gap-6 bg-bg-base p-6 md:flex-row md:items-center md:justify-between">
            <div className="flex flex-col gap-1" aria-live="polite">
              <span className="eyebrow">Votre réservation</span>
              <span className="font-serif text-2xl font-medium first-letter:uppercase">
                {date ? formatLongDate(date) : 'Choisissez une date'} · {covers} couvert{covers > 1 ? 's' : ''}
                {time ? ` · ${time}` : ''}
              </span>
            </div>
            <button type="submit" className="btn-primary shrink-0" disabled={status === 'loading'}>
              {status === 'loading' ? 'Envoi…' : 'Confirmer la réservation'} <ArrowIcon />
            </button>
          </div>
          {errors.form && <p className="text-sm text-red-300">{errors.form}</p>}
        </Reveal>
      </div>

      {confirmation && (
        <div className="fixed inset-0 z-50 flex animate-fade items-center justify-center bg-bg-base/90 p-4 backdrop-blur" role="dialog" aria-modal="true" aria-labelledby="confirm-title">
          <div className="relative flex w-full max-w-lg animate-rise flex-col items-center gap-6 border border-line-brass bg-bg-elevated p-10 text-center md:p-14">
            <button type="button" onClick={reset} className="absolute right-4 top-4 p-2 text-ink-secondary" aria-label="Fermer">
              <CloseIcon />
            </button>
            <span className="flex h-16 w-16 items-center justify-center rounded-full border border-brass-500 text-brass-500" aria-hidden="true">
              <svg viewBox="0 0 24 24" fill="none" className="h-7 w-7">
                <path d="M5 12.5l4.5 4.5L19 7.5" stroke="currentColor" strokeWidth="1.2" />
              </svg>
            </span>
            <span className="eyebrow">Réservation confirmée · {confirmation.reference}</span>
            <h3 id="confirm-title" className="font-serif text-4xl font-light">
              À très bientôt, {form.name.split(' ')[0]}.
            </h3>
            <p className="text-ink-secondary">
              Votre table est réservée le <strong className="font-medium text-ink-primary">{formatLongDate(date)}</strong> à{' '}
              <strong className="font-medium text-ink-primary">{time}</strong> pour {covers} couvert{covers > 1 ? 's' : ''}. Un e-mail de
              confirmation vous a été envoyé à {form.email}.
            </p>
            <button type="button" onClick={reset} className="btn-secondary" autoFocus>
              Fermer
            </button>
          </div>
        </div>
      )}
    </section>
  );
}
