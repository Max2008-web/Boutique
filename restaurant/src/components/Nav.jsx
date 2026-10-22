import { useEffect, useState } from 'react';
import Logo from './Logo.jsx';
import { CloseIcon } from './ui.jsx';
import { RESTAURANT } from '../data/content.js';

const LINKS = [
  { href: '#concept', label: 'La Maison' },
  { href: '#carte', label: 'La Carte' },
  { href: '#galerie', label: 'Galerie' },
  { href: '#contact', label: 'Contact' },
];

export default function Nav() {
  const [scrolled, setScrolled] = useState(false);
  const [open, setOpen] = useState(false);

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 40);
    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
    return () => window.removeEventListener('scroll', onScroll);
  }, []);

  useEffect(() => {
    document.body.style.overflow = open ? 'hidden' : '';
    const onKey = (e) => e.key === 'Escape' && setOpen(false);
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [open]);

  return (
    <header
      className={`fixed inset-x-0 top-0 z-40 pt-[env(safe-area-inset-top)] transition-all duration-500 ease-lux ${
        scrolled ? 'border-b border-line-subtle bg-bg-base/90 backdrop-blur-md' : 'border-b border-ink-primary/10 bg-transparent'
      }`}
    >
      <nav className={`container-lux grid grid-cols-[1fr_auto_1fr] items-center transition-all duration-500 ${scrolled ? 'py-4' : 'py-6 lg:py-8'}`} aria-label="Navigation principale">
        <div className="flex items-center">
          <button
            type="button"
            className="-ml-2 p-2 text-ink-primary lg:hidden"
            aria-label="Ouvrir le menu"
            aria-expanded={open}
            onClick={() => setOpen(true)}
          >
            <svg viewBox="0 0 24 24" fill="none" className="h-6 w-6" aria-hidden="true">
              <path d="M3 8h18M3 16h12" stroke="currentColor" strokeWidth="1.2" />
            </svg>
          </button>
          <ul className="hidden items-center gap-10 lg:flex">
            {LINKS.slice(0, 3).map((l) => (
              <li key={l.href}>
                <a href={l.href} className="eyebrow text-ink-primary transition-colors hover:text-brass-400">
                  {l.label}
                </a>
              </li>
            ))}
          </ul>
        </div>
        <Logo />
        <div className="flex items-center justify-end gap-8">
          <a href={RESTAURANT.phoneHref} className="hidden text-sm text-ink-secondary transition-colors hover:text-brass-400 xl:block">
            {RESTAURANT.phone}
          </a>
          <a href={RESTAURANT.phoneHref} className="p-2 text-brass-500 lg:hidden" aria-label={`Appeler le ${RESTAURANT.phone}`}>
            <svg viewBox="0 0 24 24" fill="none" className="h-[22px] w-[22px]" aria-hidden="true">
              <path d="M5 4h4l2 5-2.5 1.5a11 11 0 0 0 5 5L15 13l5 2v4a1 1 0 0 1-1 1A16 16 0 0 1 4 5a1 1 0 0 1 1-1z" stroke="currentColor" strokeWidth="1.2" />
            </svg>
          </a>
          <a href="#reservation" className="btn-secondary hidden px-6 py-3 lg:inline-flex">
            Réserver
          </a>
        </div>
      </nav>

      {open && (
        <div className="fixed inset-0 z-50 flex animate-fade flex-col bg-bg-base lg:hidden" role="dialog" aria-modal="true" aria-label="Menu">
          <div className="container-lux flex items-center justify-between py-6">
            <Logo />
            <button type="button" className="p-2 text-ink-primary" aria-label="Fermer le menu" onClick={() => setOpen(false)}>
              <CloseIcon />
            </button>
          </div>
          <ul className="container-lux mt-12 flex flex-col gap-6">
            {[...LINKS, { href: '#reservation', label: 'Réservation' }].map((l, i) => (
              <li key={l.href} className="animate-rise" style={{ animationDelay: `${i * 70}ms` }}>
                <a href={l.href} onClick={() => setOpen(false)} className="font-serif text-5xl font-light text-ink-primary">
                  {l.label}
                </a>
              </li>
            ))}
          </ul>
          <div className="container-lux mt-auto flex flex-col gap-2 pb-10 text-ink-secondary">
            <span className="eyebrow">Nous joindre</span>
            <a href={RESTAURANT.phoneHref}>{RESTAURANT.phone}</a>
            <a href={`mailto:${RESTAURANT.email}`}>{RESTAURANT.email}</a>
          </div>
        </div>
      )}
    </header>
  );
}
