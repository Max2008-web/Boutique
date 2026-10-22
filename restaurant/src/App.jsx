import Nav from './components/Nav.jsx';
import Hero from './components/Hero.jsx';
import Concept from './components/Concept.jsx';
import Menu from './components/Menu.jsx';
import Gallery from './components/Gallery.jsx';
import Reservation from './components/Reservation.jsx';
import Footer from './components/Footer.jsx';
import StickyCta from './components/StickyCta.jsx';

export default function App() {
  return (
    <>
      <a href="#main" className="sr-only focus:not-sr-only focus:fixed focus:left-4 focus:top-4 focus:z-[60] focus:bg-brass-500 focus:px-4 focus:py-2 focus:text-ink-inverse">
        Aller au contenu
      </a>
      <Nav />
      <main id="main">
        <Hero />
        <Concept />
        <Menu />
        <Gallery />
        <Reservation />
      </main>
      <Footer />
      <StickyCta />
    </>
  );
}
