import { useRef, useState } from 'react';
import { ArrowIcon, Overline, Reveal } from './ui.jsx';
import { MENU, TASTING_MENU } from '../data/content.js';

const TABS = Object.entries(MENU);

export default function Menu() {
  const [active, setActive] = useState(TABS[0][0]);
  const tabRefs = useRef([]);

  const onKeyDown = (e, i) => {
    if (!['ArrowRight', 'ArrowLeft', 'Home', 'End'].includes(e.key)) return;
    e.preventDefault();
    const next = e.key === 'Home' ? 0 : e.key === 'End' ? TABS.length - 1 : (i + (e.key === 'ArrowRight' ? 1 : -1) + TABS.length) % TABS.length;
    setActive(TABS[next][0]);
    tabRefs.current[next]?.focus();
  };

  const current = MENU[active];

  return (
    <section id="carte" aria-labelledby="carte-title" className="bg-bg-elevated py-24 md:py-40">
      <div className="container-lux flex flex-col gap-12 md:gap-16">
        <Reveal className="flex flex-col items-center gap-4 text-center">
          <Overline center>La carte & les menus</Overline>
          <h2 id="carte-title" className="font-serif text-5xl font-light leading-[1.05] md:text-[64px]">
            Au rythme des saisons
          </h2>
          <p className="font-serif text-xl italic text-ink-secondary md:text-[22px]">Carte d’automne · servie du mardi au samedi, midi et soir</p>
        </Reveal>

        <div role="tablist" aria-label="Sections de la carte" className="-mx-6 flex gap-8 overflow-x-auto border-b border-line-subtle px-6 md:mx-0 md:justify-center md:gap-12 md:border-0 md:px-0">
          {TABS.map(([key, { label }], i) => (
            <button
              key={key}
              ref={(el) => (tabRefs.current[i] = el)}
              role="tab"
              id={`tab-${key}`}
              aria-selected={active === key}
              aria-controls={`panel-${key}`}
              tabIndex={active === key ? 0 : -1}
              onClick={() => setActive(key)}
              onKeyDown={(e) => onKeyDown(e, i)}
              className={`shrink-0 whitespace-nowrap border-b-2 py-3 font-serif text-2xl font-medium transition-colors duration-300 ${
                active === key ? 'border-brass-500 text-ink-primary' : 'border-transparent text-ink-muted hover:text-ink-secondary'
              }`}
            >
              {label}
            </button>
          ))}
        </div>

        <div className="grid gap-16 lg:grid-cols-[1fr_400px] lg:gap-20">
          <div role="tabpanel" id={`panel-${active}`} aria-labelledby={`tab-${active}`} key={active} className="flex animate-fade flex-col gap-8">
            <ul className="grid gap-x-16 md:grid-cols-2">
              {current.items.map((item) => (
                <li key={item.name} className="flex flex-col gap-2 border-b border-line-subtle py-6">
                  <div className="flex items-center gap-4">
                    <h3 className="font-serif text-2xl font-medium leading-tight">{item.name}</h3>
                    <span className="h-px flex-1 border-t border-dashed border-line-brass" aria-hidden="true" />
                    <span className="whitespace-nowrap font-serif text-[22px] font-medium text-brass-500">
                      {item.price}
                      <span className="sr-only"> euros</span>
                    </span>
                  </div>
                  <p className="text-sm leading-relaxed text-ink-secondary">{item.desc}</p>
                </li>
              ))}
            </ul>
            <p className="text-sm text-ink-muted">Prix nets en euros, service compris. Allergènes disponibles sur demande.</p>
          </div>

          <Reveal as="aside" className="flex flex-col gap-6 self-start border border-line-brass bg-bg-base p-8 md:px-10 md:py-12" aria-label="Menu signature">
            <span className="eyebrow">Menu signature</span>
            <h3 className="max-w-[260px] font-serif text-4xl leading-[1.1] md:text-5xl">{TASTING_MENU.title}</h3>
            <ol className="flex flex-col gap-3 border-y border-line-subtle py-6">
              {TASTING_MENU.steps.map((s, i) => (
                <li key={s} className="flex items-center gap-3 text-sm text-ink-secondary">
                  <span className="w-5 font-serif text-base text-brass-500">{String(i + 1).padStart(2, '0')}</span>
                  {s}
                </li>
              ))}
            </ol>
            <div className="flex items-baseline justify-between">
              <span className="font-serif text-5xl text-brass-500">{TASTING_MENU.price} €</span>
              <span className="text-sm text-ink-secondary">Accord mets-vins + {TASTING_MENU.pairing} €</span>
            </div>
            <a href="#reservation" className="btn-primary w-full">
              Réserver ce menu <ArrowIcon />
            </a>
          </Reveal>
        </div>
      </div>
    </section>
  );
}
