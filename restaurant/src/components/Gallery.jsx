import { useCallback, useEffect, useState } from 'react';
import { ChevronIcon, CloseIcon, Photo, Reveal, SectionHeader } from './ui.jsx';
import { GALLERY, PHOTO_ALT } from '../data/content.js';

const PHOTOS = GALLERY.filter((g) => g.key);

export default function Gallery() {
  const [index, setIndex] = useState(null);
  const close = useCallback(() => setIndex(null), []);
  const step = useCallback((d) => setIndex((i) => (i + d + PHOTOS.length) % PHOTOS.length), []);

  useEffect(() => {
    if (index === null) return;
    const onKey = (e) => {
      if (e.key === 'Escape') close();
      if (e.key === 'ArrowRight') step(1);
      if (e.key === 'ArrowLeft') step(-1);
    };
    document.body.style.overflow = 'hidden';
    window.addEventListener('keydown', onKey);
    return () => {
      document.body.style.overflow = '';
      window.removeEventListener('keydown', onKey);
    };
  }, [index, close, step]);

  return (
    <section id="galerie" aria-labelledby="galerie-title" className="container-lux flex flex-col gap-12 py-24 md:gap-16 md:py-40">
      <Reveal className="flex flex-col justify-between gap-6 md:flex-row md:items-end">
        <SectionHeader id="galerie-title" overline="Galerie" title="L’assiette, la salle, la cave" />
        <p className="max-w-[400px] text-ink-secondary">
          Un voyage visuel dans l’univers de la Maison — lumière tamisée, laiton patiné et gestes précis.
        </p>
      </Reveal>

      <div className="grid auto-rows-[200px] grid-cols-2 gap-2 md:auto-rows-[252px] md:grid-cols-4 md:gap-4">
        {GALLERY.map((g, i) =>
          g.quote ? (
            <Reveal key="quote" delay={i * 60} className={`flex flex-col justify-between bg-brass-500 p-5 text-ink-inverse md:p-8 ${g.className}`}>
              <p className="font-serif text-xl leading-tight md:text-3xl">{g.quote}</p>
              <span className="text-[10px] font-semibold uppercase tracking-[0.24em] md:text-overline">{g.source}</span>
            </Reveal>
          ) : (
            <Reveal key={g.key} delay={i * 60} className={`${i === 0 ? 'col-span-2' : ''} ${g.className}`}>
              <button
                type="button"
                onClick={() => setIndex(PHOTOS.findIndex((p) => p.key === g.key))}
                className="group relative block h-full w-full overflow-hidden text-left"
                aria-label={`Agrandir : ${g.caption}`}
              >
                <Photo name={g.key} alt={PHOTO_ALT[g.key]} className="h-full w-full" imgClassName="transition-transform duration-[1.2s] ease-lux group-hover:scale-105" />
                <span className="absolute inset-0 bg-gradient-to-b from-transparent via-transparent to-bg-base/80" />
                <span className="absolute bottom-4 left-4 text-[10px] font-semibold uppercase tracking-[0.24em] text-ink-primary md:bottom-6 md:left-6 md:text-overline">
                  {g.caption}
                </span>
              </button>
            </Reveal>
          ),
        )}
      </div>

      {index !== null && (
        <div className="fixed inset-0 z-50 flex animate-fade items-center justify-center bg-bg-base/95 p-4 backdrop-blur" role="dialog" aria-modal="true" aria-label={PHOTOS[index].caption} onClick={close}>
          <button type="button" onClick={close} className="absolute right-4 top-4 p-3 text-ink-primary md:right-8 md:top-8" aria-label="Fermer">
            <CloseIcon />
          </button>
          <button type="button" onClick={(e) => (e.stopPropagation(), step(-1))} className="absolute left-2 p-3 text-brass-500 md:left-8" aria-label="Photo précédente">
            <ChevronIcon dir="left" className="h-8 w-8" />
          </button>
          <figure className="flex max-h-full w-full max-w-5xl flex-col gap-4" onClick={(e) => e.stopPropagation()}>
            <Photo key={PHOTOS[index].key} name={PHOTOS[index].key} alt={PHOTO_ALT[PHOTOS[index].key]} className="max-h-[78vh] w-full" imgClassName="object-contain" />
            <figcaption className="flex justify-between">
              <span className="eyebrow">{PHOTOS[index].caption}</span>
              <span className="text-sm text-ink-muted">
                {index + 1} / {PHOTOS.length}
              </span>
            </figcaption>
          </figure>
          <button type="button" onClick={(e) => (e.stopPropagation(), step(1))} className="absolute right-2 p-3 text-brass-500 md:right-8" aria-label="Photo suivante">
            <ChevronIcon className="h-8 w-8" />
          </button>
        </div>
      )}
    </section>
  );
}
