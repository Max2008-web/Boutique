import { useState } from 'react';

export default function App() {
  const [input, setInput] = useState('');
  const [quote, setQuote] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [copied, setCopied] = useState(false);

  async function handleGenerate() {
    if (!input.trim()) {
      setError('Veuillez décrire les travaux à réaliser.');
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
      setError('Impossible de copier dans le presse-papier.');
    }
  }

  return (
    <div className="min-h-screen bg-slate-50 text-slate-800">
      <div className="mx-auto max-w-3xl px-4 py-10">
        <header className="mb-8">
          <h1 className="text-3xl font-bold text-slate-900">
            Générateur de devis
          </h1>
          <p className="mt-2 text-slate-600">
            Décrivez les travaux en langage courant. L'assistant rédige un devis
            professionnel et structuré.
          </p>
        </header>

        <section className="rounded-2xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
          <label htmlFor="works" className="block text-sm font-medium text-slate-700">
            Liste des travaux
          </label>
          <textarea
            id="works"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            rows={8}
            placeholder="Ex : refaire la salle de bain de 6m², carrelage au sol et aux murs, nouveau lavabo, changer la robinetterie, peindre le plafond..."
            className="mt-2 w-full resize-y rounded-xl border border-slate-300 bg-white p-3 text-slate-800 outline-none focus:border-slate-500 focus:ring-2 focus:ring-slate-200"
          />

          <div className="mt-4 flex items-center gap-3">
            <button
              onClick={handleGenerate}
              disabled={loading}
              className="inline-flex items-center justify-center rounded-xl bg-slate-900 px-5 py-2.5 text-sm font-semibold text-white shadow hover:bg-slate-800 disabled:cursor-not-allowed disabled:opacity-60"
            >
              {loading ? 'Génération en cours…' : 'Générer'}
            </button>
            {error && <span className="text-sm text-red-600">{error}</span>}
          </div>
        </section>

        {quote && (
          <section className="mt-6 rounded-2xl bg-white p-6 shadow-sm ring-1 ring-slate-200">
            <div className="mb-3 flex items-center justify-between">
              <h2 className="text-lg font-semibold text-slate-900">
                Devis généré
              </h2>
              <button
                onClick={handleCopy}
                className="inline-flex items-center rounded-lg border border-slate-300 bg-white px-3 py-1.5 text-sm font-medium text-slate-700 shadow-sm hover:bg-slate-100"
              >
                {copied ? 'Copié !' : 'Copier'}
              </button>
            </div>
            <pre className="whitespace-pre-wrap break-words font-sans text-slate-800">
              {quote}
            </pre>
          </section>
        )}

        <footer className="mt-10 text-center text-xs text-slate-400">
          Les montants proposés sont indicatifs. Vérifiez et ajustez selon votre
          chantier avant envoi au client.
        </footer>
      </div>
    </div>
  );
}
