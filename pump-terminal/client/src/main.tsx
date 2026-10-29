import "./polyfills";
import { StrictMode, useMemo } from "react";
import { createRoot } from "react-dom/client";
import { BrowserRouter } from "react-router-dom";
import { ConnectionProvider, WalletProvider } from "@solana/wallet-adapter-react";
import { WalletModalProvider } from "@solana/wallet-adapter-react-ui";
import "@solana/wallet-adapter-react-ui/styles.css";
import "./index.css";
import App from "./App";
import { useSettings } from "./lib/settings";

function Root() {
  const token = useSettings((s) => s.accessToken);
  const endpoint = `${location.origin}/api/rpc`;
  const config = useMemo(
    () => ({ commitment: "confirmed" as const, httpHeaders: token ? { "x-access-token": token } : undefined }),
    [token],
  );
  // Phantom est détecté automatiquement (Wallet Standard). Sur Android, le Mobile Wallet Adapter est ajouté automatiquement.
  const wallets = useMemo(() => [], []);
  return (
    <ConnectionProvider endpoint={endpoint} config={config}>
      <WalletProvider wallets={wallets} autoConnect>
        <WalletModalProvider>
          <BrowserRouter>
            <App />
          </BrowserRouter>
        </WalletModalProvider>
      </WalletProvider>
    </ConnectionProvider>
  );
}

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <Root />
  </StrictMode>,
);
