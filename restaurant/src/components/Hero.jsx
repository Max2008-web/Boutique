import { useMemo } from 'react';
import { ArrowIcon, Photo } from './ui.jsx';
import { PHOTO_ALT } from '../data/content.js';
import { isDateBookable, remainingTables, toISODate } from '../lib/booking.js';

function tonight() {
  const now = new Date();
  if (!isDateBookable(now, now)) return { label: 'Prochain service', line: 'Mardi · dès 12:00', note: 'Fermé dimanche & lundi' };
  const left = remainingTables(toISODate(now), now);
  return {
    label: 'Ce soir',
    line: 'Dîner · 19:00 — 22:30',
    note: left > 0 ? `Encore ${left} table${left > 1 ? 's' : ''} disponible${left > 1 ? 's' : ''}` : 'Complet — liste d’attente au téléphone',
  };
}

export default function Hero() {
  const info = useMemo(tonight, []);
  return (
    <section id="top" className="relative flex h-[100svh] min-h-[640px] max-h-[1100px] flex-col justify-end overflow-hidden" aria-label="Accueil">
      <Photo name="hero" alt={PHOTO_ALT.hero} eager className="!absolute inset-0 animate-zoom" />
      <div className="absolute inset-0 bg-gradient-to-r from-bg-base/95 via-bg-base/50 to-bg-base/10" />
      <div className="absolute inset-0 bg-gradient-to-b from-bg-base/60 via-transparent to-bg-base" />

      <div className="container-lux relative z-10 flex flex-col gap-10 pb-24 pt-40 lg:flex-row lg:items-end lg:justify-between">
        <div className="flex max-w-[820px] flex-col gap-8">
          <div className="flex animate-rise items-center gap-4">
            <span className="h-px w-12 bg-brass-500" />
            <span className="eyebrow">Cuisine d’auteur · Paris 8ᵉ</span>
          </div>
          <h1 className="animate-rise font-serif text-[48px] font-light leading-[1] tracking-[-0.02em] text-ink-primary [animation-delay:120ms] sm:text-7xl lg:text-[120px]">
            L’art du feu,
            <br />
            la grâce du goût.
          </h1>
          <p className="max-w-[560px] animate-rise text-base leading-relaxed text-ink-secondary [animation-delay:240ms] md:text-lg">
            Une table intimiste de 32 couverts où le Chef Aurèle Vasseur compose, au fil des saisons, une cuisine de produits rares
            sublimée par la braise.
          </p>
          <div className="flex animate-rise flex-col gap-4 [animation-delay:360ms] sm:flex-row sm:items-center sm:gap-8">
            <a href="#reservation" className="btn-primary">
              Réserver une table <ArrowIcon />
            </a>
            <a href="#carte" className="btn-secondary sm:hidden">
              Découvrir la carte <ArrowIcon />
            </a>
            <a href="#carte" className="btn-ghost hidden sm:inline-flex">
              Découvrir la carte <ArrowIcon />
            </a>
          </div>
        </div>
        <aside className="hidden animate-rise flex-col gap-3 border border-line-brass bg-bg-base/60 px-8 py-6 backdrop-blur-sm [animation-delay:500ms] lg:flex">
          <span className="eyebrow">{info.label}</span>
          <span className="font-serif text-2xl font-medium">{info.line}</span>
          <span className="text-sm text-ink-secondary">{info.note}</span>
        </aside>
      </div>
    </section>
  );
}
