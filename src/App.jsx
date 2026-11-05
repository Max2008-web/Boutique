import { useState } from 'react';

export default function App() {
  const [input, setInput] = useState('');
  const [quote, setQuote] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [copied, setCopied] = useState(false);

  async function handleGenerate() {
    if (!input.trim()) {
      setError('Décrivez d’abord les travaux à réaliser.');
      return;
    }
    setError('');
    setQuote('');
    setLoading(true);
    try {
      const res = await fetch('/api/generate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ description: input }),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || `Erreur ${res.status}`);
      }
      const data = await res.json();
      setQuote(data.quote);
    } catch (e) {
      setError(e.message || 'Une erreur est survenue.');
    } finally {
      setLoading(false);
    }
  }

  async function handleCopy() {
    try {
      await navigator.clipboard.writeText(quote);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    } catch {
      setError('Copie impossible : sélectionnez le texte du devis à la main.');
    }
  }

  function handleKeyDown(e) {
    if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) {
      e.preventDefault();
      if (!loading) handleGenerate();
    }
  }

  return (
    <div className="min-h-screen">
      <div className="mx-auto max-w-6xl px-4 pb-12 pt-10 sm:px-8 sm:pt-14">
        <header className="max-w-2xl">
          <h1 className="text-5xl font-extrabold leading-none tracking-tight sm:text-7xl">
            Devis
          </h1>
          <p className="mt-4 text-lg leading-relaxed text-graphite/80">
            Notez les travaux comme sur le chantier. Le devis est rédigé poste
            par poste, chiffré et prêt à coller dans votre logiciel de
            facturation.
          </p>
        </header>

        <main className="mt-10 grid gap-10 lg:grid-cols-[minmax(0,4fr)_minmax(0,7fr)] lg:gap-12">
          <section aria-labelledby="notes-titre">
            <h2 id="notes-titre" className="text-xl font-bold">
              <label htmlFor="works">Notes de chantier</label>
            </h2>
            <p className="mt-1 text-sm text-graphite/70">
              Surfaces, matériaux, ce qui est à déposer : en vrac, ça suffit.
            </p>
            <textarea
              id="works"
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={handleKeyDown}
              rows={12}
              placeholder="Salle de bain 6 m², carrelage sol et murs, nouveau lavabo, changer la robinetterie, peindre le plafond…"
              className="quadrille mt-4 block w-full resize-y rounded-sm border-2 border-graphite px-4 py-3 text-base text-graphite placeholder:text-graphite/45 focus:border-carbone focus:outline-none"
              style={{ lineHeight: '20px', paddingTop: '10px' }}
            />

            <div className="mt-5 flex flex-wrap items-center gap-x-4 gap-y-2">
              <button
                onClick={handleGenerate}
                disabled={loading}
                className="rounded-sm bg-carbone px-6 py-3 text-base font-bold text-white transition-colors hover:bg-graphite disabled:cursor-wait disabled:opacity-70"
              >
                {loading ? 'Rédaction en cours…' : 'Rédiger le devis'}
              </button>
              <span className="hidden text-sm text-graphite/60 sm:inline">
                ou Ctrl + Entrée
              </span>
            </div>
            {error && (
              <p role="alert" className="mt-4 border-l-4 border-red-700 pl-3 text-sm font-medium text-red-800">
                {error}
              </p>
            )}
          </section>

          <section aria-labelledby="devis-titre" aria-live="polite" aria-busy={loading}>
            <div
              key={quote ? 'rempli' : 'vide'}
              className={`flex min-h-[26rem] shadow-[6px_8px_0_rgba(43,45,49,0.12)] ${quote ? 'arrivee' : ''}`}
            >
              <div className="w-8 shrink-0 bg-souche sm:w-12" aria-hidden="true" />
              <div className="perforation w-[14px] shrink-0 bg-double" aria-hidden="true" />
              <div className="min-w-0 flex-1 bg-double px-5 py-5 sm:px-7 sm:py-6">
                <div className="flex flex-wrap items-baseline justify-between gap-3 border-b-2 border-carbone/25 pb-3">
                  <h2 id="devis-titre" className="text-xl font-bold text-carbone">
                    Devis rédigé
                  </h2>
                  {quote && (
                    <button
                      onClick={handleCopy}
                      className="rounded-sm border-2 border-carbone px-3 py-1.5 text-sm font-bold text-carbone transition-colors hover:bg-carbone hover:text-double"
                    >
                      {copied ? 'Devis copié' : 'Copier le devis'}
                    </button>
                  )}
                </div>

                {quote ? (
                  <pre className="mt-4 whitespace-pre-wrap break-words font-machine text-[15px] lg:text-[14px] leading-relaxed text-carbone">
                    {quote}
                  </pre>
                ) : (
                  <p className="mt-6 max-w-sm font-machine text-[15px] leading-relaxed text-carbone/70">
                    {loading
                      ? 'Le devis est en cours de rédaction. Comptez une vingtaine de secondes.'
                      : 'Le devis s’inscrira sur cette feuille. Commencez par noter les travaux.'}
                  </p>
                )}
              </div>
            </div>
          </section>
        </main>

        <footer className="mt-12 max-w-2xl text-sm text-graphite/60">
          Les montants sont indicatifs. Relisez et ajustez selon votre chantier
          avant l’envoi au client.
        </footer>
      </div>
    </div>
  );
}
