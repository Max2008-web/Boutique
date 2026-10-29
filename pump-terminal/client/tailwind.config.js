/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        bg: "#08090c",
        panel: "#0f1217",
        panel2: "#151920",
        line: "#20252e",
        muted: "#7d8590",
        up: "#16c784",
        down: "#ea3943",
        brand: "#7c5cff",
        warn: "#f5a524",
      },
      fontFamily: {
        sans: ["Inter", "system-ui", "-apple-system", "Segoe UI", "Roboto", "sans-serif"],
        mono: ["JetBrains Mono", "ui-monospace", "SFMono-Regular", "Menlo", "monospace"],
      },
    },
  },
  plugins: [],
};
