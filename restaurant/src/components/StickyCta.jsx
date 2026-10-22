import { useEffect, useState } from 'react';

// Barre de réservation fixe sur mobile : apparaît après le hero, disparaît sur la section réservation.
export default function StickyCta() {
  const [show, setShow] = useState(false);
  useEffect(() => {
    const onScroll = () => {
      const resa = document.getElementById('reservation');
      const r = resa?.getBoundingClientRect();
      const inResa = r && r.top < window.innerHeight && r.bottom > 0;
      setShow(window.scrollY > window.innerHeight * 0.8 && !inResa);
    };
    onScroll();
    window.addEventListener('scroll', onScroll, { passive: true });
    return () => window.removeEventListener('scroll', onScroll);
  }, []);
  return (
    <div
      className={`fixed inset-x-0 bottom-0 z-30 flex items-center justify-between gap-3 border-t border-line-subtle bg-bg-base/95 px-4 py-3 backdrop-blur transition-transform duration-500 ease-lux lg:hidden ${
        show ? 'translate-y-0' : 'translate-y-full'
      }`}
      aria-hidden={!show}
    >
      <div className="flex flex-col">
        <span className="text-[10px] font-semibold uppercase tracking-[0.24em] text-brass-500">Mar — Sam</span>
        <span className="text-sm text-ink-primary">Déjeuner & dîner</span>
      </div>
      <a href="#reservation" className="btn-primary px-6 py-3" tabIndex={show ? 0 : -1}>
        Réserver
      </a>
    </div>
  );
}
