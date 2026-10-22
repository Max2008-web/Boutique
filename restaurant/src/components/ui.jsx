import { useEffect, useRef, useState } from 'react';

export const ArrowIcon = ({ className = 'h-4 w-4' }) => (
  <svg viewBox="0 0 16 16" fill="none" className={className} aria-hidden="true">
    <path d="M1 8h13M9 3l5 5-5 5" stroke="currentColor" strokeWidth="1.2" />
  </svg>
);

export const ChevronIcon = ({ dir = 'right', className = 'h-4 w-4' }) => (
  <svg viewBox="0 0 16 16" fill="none" className={className} aria-hidden="true">
    <path d={dir === 'right' ? 'M6 3l5 5-5 5' : 'M10 3L5 8l5 5'} stroke="currentColor" strokeWidth="1.2" />
  </svg>
);

export const CloseIcon = ({ className = 'h-6 w-6' }) => (
  <svg viewBox="0 0 24 24" fill="none" className={className} aria-hidden="true">
    <path d="M5 5l14 14M19 5L5 19" stroke="currentColor" strokeWidth="1.2" />
  </svg>
);

export function Overline({ children, center = false, className = '' }) {
  return (
    <div className={`flex items-center gap-4 ${center ? 'justify-center' : ''} ${className}`}>
      <span className="h-px w-10 bg-brass-500" />
      <span className="eyebrow">{children}</span>
      {center && <span className="h-px w-10 bg-brass-500" />}
    </div>
  );
}

export function SectionHeader({ overline, title, center = false, id }) {
  return (
    <div className={`flex flex-col gap-4 ${center ? 'items-center text-center' : ''}`}>
      <Overline center={center}>{overline}</Overline>
      <h2 id={id} className="font-serif text-4xl font-normal leading-[1.1] tracking-[-0.01em] text-ink-primary md:text-5xl">
        {title}
      </h2>
    </div>
  );
}

// Image avec placeholder flou (LQIP) : affiche l'aperçu basse définition tant que
// la photo HD n'est pas chargée, et le conserve si le fichier HD est absent.
export function Photo({ name, alt, className = '', imgClassName = '', eager = false }) {
  const [state, setState] = useState('loading');
  return (
    <div className={`relative overflow-hidden bg-bg-surface ${className}`}>
      <img
        src={`/images/lqip/${name}.jpg`}
        alt=""
        aria-hidden="true"
        className={`absolute inset-0 h-full w-full scale-105 object-cover transition-opacity duration-700 ${
          state === 'loaded' ? 'opacity-0' : 'opacity-100'
        } ${state === 'fallback' ? 'blur-[1px]' : 'blur-lg'}`}
      />
      <img
        src={`/images/${name}.jpg`}
        alt={alt}
        loading={eager ? 'eager' : 'lazy'}
        decoding="async"
        onLoad={() => setState('loaded')}
        onError={() => setState('fallback')}
        className={`relative h-full w-full object-cover transition-opacity duration-700 ${
          state === 'loaded' ? 'opacity-100' : 'opacity-0'
        } ${imgClassName}`}
      />
    </div>
  );
}

export function Reveal({ as: Tag = 'div', delay = 0, className = '', children, ...rest }) {
  const ref = useRef(null);
  const [visible, setVisible] = useState(false);
  useEffect(() => {
    const el = ref.current;
    if (!el || !('IntersectionObserver' in window)) return setVisible(true);
    const io = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) {
          setVisible(true);
          io.disconnect();
        }
      },
      { threshold: 0.15, rootMargin: '0px 0px -40px 0px' },
    );
    io.observe(el);
    return () => io.disconnect();
  }, []);
  return (
    <Tag ref={ref} className={`reveal ${visible ? 'is-visible' : ''} ${className}`} style={{ transitionDelay: `${delay}ms` }} {...rest}>
      {children}
    </Tag>
  );
}
