import { Photo, Reveal, SectionHeader } from './ui.jsx';
import { PHOTO_ALT, RESTAURANT } from '../data/content.js';

const STATS = [
  ['32', 'Couverts'],
  ['7', 'Temps du menu'],
  ['640', 'Vins en cave'],
];

export default function Concept() {
  return (
    <section id="concept" aria-labelledby="concept-title" className="container-lux grid items-center gap-16 py-24 md:py-40 lg:grid-cols-[560px_1fr] lg:gap-30">
      <Reveal className="relative mr-6 mt-6 lg:mr-10 lg:mt-10">
        <div className="absolute -bottom-6 -left-0 right-6 top-6 border border-brass-500 lg:-bottom-10 lg:right-10 lg:top-10" aria-hidden="true" />
        <Photo name="chef" alt={PHOTO_ALT.chef} className="relative ml-6 aspect-[13/17] lg:ml-10" />
        <div className="absolute -bottom-6 right-0 flex flex-col items-center gap-1 bg-brass-500 p-4 text-ink-inverse lg:-bottom-10 lg:-right-10 lg:p-6">
          <span className="font-serif text-4xl font-light leading-none lg:text-[64px]">18</span>
          <span className="text-[10px] font-semibold tracking-[0.24em] lg:text-overline">ANS DE MÉTIER</span>
        </div>
      </Reveal>

      <div className="flex flex-col gap-10">
        <Reveal>
          <SectionHeader id="concept-title" overline="Concept & Savoir-faire" title="Une cuisine de la lumière et du feu" />
        </Reveal>
        <Reveal delay={100}>
          <p className="max-w-[600px] text-base leading-relaxed text-ink-secondary md:text-lg">
            Chez Aurèle, chaque assiette naît d’une rencontre : un pêcheur de l’île d’Yeu, une maraîchère du Vexin, un vigneron de
            Bourgogne. Le Chef travaille la braise, le fumage et les cuissons lentes pour révéler l’essence de produits d’exception.
          </p>
        </Reveal>
        <Reveal delay={200} as="blockquote" className="flex gap-6 border-l border-brass-500 pl-6">
          <p className="max-w-[560px] font-serif text-2xl italic leading-snug text-ink-primary md:text-[28px]">
            « Le luxe, c’est le temps que l’on accorde au produit. Le reste n’est que décor. »
          </p>
        </Reveal>
        <Reveal delay={250} className="flex flex-col gap-1">
          <span className="font-serif text-3xl text-brass-500">{RESTAURANT.chef}</span>
          <span className="eyebrow text-ink-muted">Chef exécutif & fondateur</span>
        </Reveal>
        <Reveal delay={300} as="dl" className="grid grid-cols-3 border-t border-line-subtle">
          {STATS.map(([n, l]) => (
            <div key={l} className="flex flex-col-reverse gap-2 pt-8">
              <dt className="eyebrow text-[10px] text-ink-muted md:text-overline">{l}</dt>
              <dd className="font-serif text-4xl font-light md:text-[64px] md:leading-none">{n}</dd>
            </div>
          ))}
        </Reveal>
      </div>
    </section>
  );
}
