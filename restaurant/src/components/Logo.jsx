export default function Logo({ className = '' }) {
  return (
    <a href="#top" className={`flex flex-col items-center leading-none ${className}`} aria-label="Maison Aurèle — accueil">
      <span className="font-serif text-[26px] font-medium tracking-[0.28em] text-ink-primary">AURÈLE</span>
      <span className="mt-1 font-sans text-[8px] font-semibold tracking-[0.4em] text-brass-500">MAISON GASTRONOMIQUE</span>
    </a>
  );
}
