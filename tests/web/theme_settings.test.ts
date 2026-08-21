import {
  normalizeActiveTheme,
  normalizeBackupPanelSettings,
  normalizeThemeAutoStrategy,
  normalizeThemeMode,
  normalizeTimeOfDay,
} from "../../src/webserver/model/settings";
import { previewThemeColors } from "../../src/webserver/state/ui_tokens";

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

export function runThemeSettingsTests(): void {
  assert(normalizeThemeMode("Light") === "light", "Light mode normalizes");
  assert(normalizeThemeMode("Auto") === "auto", "Auto mode normalizes");
  assert(normalizeThemeMode("unknown") === "dark", "unknown mode fails Dark");
  assert(normalizeThemeAutoStrategy("Sunrise / Sunset") === "sunrise_sunset",
    "solar strategy normalizes");
  assert(normalizeThemeAutoStrategy("invalid") === "time",
    "unknown strategy falls back to Time");
  assert(normalizeActiveTheme("Light") === "light" && normalizeActiveTheme("") === "dark",
    "effective theme remains a two-state value");
  assert(normalizeTimeOfDay("23:45", "06:00") === "23:45" &&
    normalizeTimeOfDay("25:00", "06:00") === "06:00", "theme times are validated");

  const restored = normalizeBackupPanelSettings({}, {
    timezone: "UTC (GMT+0)", language: "en", clockFormat: "24h",
    clockFormatOptions: ["12h", "24h"], ntpDefaults: [], ntpServer1: "",
    ntpServer2: "", ntpServer3: "", coverArtHomeAssistantProtocol: "http",
    coverArtHomeAssistantPort: 8123, autoUpdate: false, updateFrequency: "Daily",
    updateFrequencyOptions: ["Daily"], screenRotationOptions: ["0"],
  });
  assert(restored.themeMode === "dark" && restored.themeAutoStrategy === "time",
    "legacy backups retain Dark defaults");
  assert(restored.themeLightStart === "06:00" && restored.themeDarkStart === "18:00",
    "legacy backups receive stable schedule defaults");

  assert(previewThemeColors("dark").background === "000000",
    "Dark preview preserves the existing background");
  assert(previewThemeColors("light").background === "F2F2F2",
    "Light preview uses the centralized baseline palette");
}
