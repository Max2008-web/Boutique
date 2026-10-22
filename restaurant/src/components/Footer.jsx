import Logo from './Logo.jsx';
import { RESTAURANT } from '../data/content.js';

export default function Footer() {
  const year = new Date().getFullYear();
  return (
    <footer id="contact" className="overflow-hidden bg-bg-base pt-20 md:pt-30">
      <div className="container-lux grid gap-12 md:grid-cols-2 lg:grid-cols-[360px_1fr_1fr_1fr] lg:gap-16">
        <div className="flex flex-col items-start gap-6">
          <Logo />
          <p className="max-w-[280px] text-sm leading-relaxed text-ink-secondary">Maison gastronomique, cuisine d’auteur à la braise. Paris 8ᵉ.</p>
        </div>
        <address className="flex flex-col gap-4 not-italic">
          <span className="eyebrow">Adresse</span>
          <a href={RESTAURANT.mapsUrl} target="_blank" rel="noreferrer" className="text-ink-secondary transition-colors hover:text-brass-400">
            {RESTAURANT.address[0]}
            <br />
            {RESTAURANT.address[1]}
          </a>
          <span className="text-ink-secondary">{RESTAURANT.access}</span>
        </address>
        <div className="flex flex-col gap-4">
          <span className="eyebrow">Horaires</span>
          <span className="text-ink-secondary">Mardi — Samedi</span>
          <span className="text-ink-secondary">Déjeuner 12:00 — 13:30</span>
          <span className="text-ink-secondary">Dîner 19:00 — 21:30</span>
          <span className="text-ink-muted">Fermé dimanche & lundi</span>
        </div>
        <div className="flex flex-col gap-4">
          <span className="eyebrow">Contact</span>
          <a href={RESTAURANT.phoneHref} className="text-ink-secondary transition-colors hover:text-brass-400">
            {RESTAURANT.phone}
          </a>
          <a href={`mailto:${RESTAURANT.email}`} className="text-ink-secondary transition-colors hover:text-brass-400">
            {RESTAURANT.email}
          </a>
          <span className="text-ink-secondary">Instagram · {RESTAURANT.instagram}</span>
          <a href={`mailto:${RESTAURANT.email}?subject=Privatisation`} className="text-ink-secondary transition-colors hover:text-brass-400">
            Privatisation & événements
          </a>
        </div>
      </div>
      <p className="select-none whitespace-nowrap py-10 text-center font-serif text-[22vw] font-light leading-none tracking-[0.2em] text-brass-500/10 lg:text-[240px]" aria-hidden="true">
        AURÈLE
      </p>
      <div className="container-lux flex flex-col gap-2 border-t border-line-subtle py-6 pb-24 text-sm text-ink-muted md:flex-row md:justify-between lg:pb-6">
        <span>© {year} Maison Aurèle — Tous droits réservés</span>
        <span>Mentions légales · Confidentialité · Cookies</span>
      </div>
    </footer>
  );
}
