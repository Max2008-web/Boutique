/** @type {import('tailwindcss').Config} */
// Tokens miroirs des variables Figma « Restaurant - UI/UX » (collections Color & Spacing 8pt).
export default {
  content: ['./index.html', './src/**/*.{js,jsx}'],
  theme: {
    extend: {
      colors: {
        bg: { base: '#0B0A09', elevated: '#141210', surface: '#1C1916', overlay: '#24201C' },
        ink: { primary: '#F3ECDF', secondary: '#BFB5A5', muted: '#7E7569', inverse: '#0B0A09' },
        brass: { 200: '#EADBB8', 400: '#D8BA7A', 500: '#C6A15B', 600: '#A8843F' },
        line: { subtle: '#2E2925', brass: '#6E5A35' },
      },
      fontFamily: {
        serif: ['"Cormorant Garamond"', 'Georgia', 'serif'],
        sans: ['Manrope', 'system-ui', 'sans-serif'],
      },
      fontSize: {
        overline: ['12px', { lineHeight: '1.4', letterSpacing: '0.24em' }],
        btn: ['13px', { lineHeight: '1', letterSpacing: '0.16em' }],
      },
      spacing: { 18: '4.5rem', 30: '7.5rem', 40: '10rem' },
      transitionTimingFunction: { lux: 'cubic-bezier(.22,.61,.36,1)' },
      keyframes: {
        rise: { from: { opacity: 0, transform: 'translateY(24px)' }, to: { opacity: 1, transform: 'none' } },
        fade: { from: { opacity: 0 }, to: { opacity: 1 } },
        zoom: { from: { transform: 'scale(1.08)' }, to: { transform: 'scale(1)' } },
      },
      animation: {
        rise: 'rise 1s cubic-bezier(.22,.61,.36,1) both',
        fade: 'fade .5s ease both',
        zoom: 'zoom 2.4s cubic-bezier(.22,.61,.36,1) both',
      },
    },
  },
  plugins: [],
};
