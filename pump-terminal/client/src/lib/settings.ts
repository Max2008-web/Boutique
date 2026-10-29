import { create } from "zustand";
import { persist } from "zustand/middleware";

export interface Settings {
  accessToken: string;
  slippagePct: number;
  priorityFeeSol: number;
  route: "pumpportal" | "jupiter";
  pool: "auto" | "pump" | "pump-amm";
  buyPresets: number[];
  notifications: boolean;
  sound: boolean;
  set: (patch: Partial<Omit<Settings, "set">>) => void;
}

export const useSettings = create<Settings>()(
  persist(
    (set) => ({
      accessToken: "",
      slippagePct: 15,
      priorityFeeSol: 0.0005,
      route: "pumpportal",
      pool: "auto",
      buyPresets: [0.05, 0.1, 0.25, 0.5],
      notifications: false,
      sound: true,
      set: (patch) => set(patch),
    }),
    { name: "pump-terminal-settings" },
  ),
);
