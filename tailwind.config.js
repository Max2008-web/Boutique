/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        papier: '#FCFCFA',
        quadrillage: '#D5DEE8',
        graphite: '#2B2D31',
        carbone: '#27358A',
        double: '#F6E98A',
        souche: '#E9DB6C',
      },
      fontFamily: {
        sans: ['"Bricolage Grotesque"', 'system-ui', 'sans-serif'],
        machine: ['"Courier Prime"', 'Courier', 'monospace'],
      },
    },
  },
  plugins: [],
};
