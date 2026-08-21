export const WEB_UI_COLORS = {
  primary: "FF8C00",
  secondary: "313131",
  tertiary: "212121",
} as const;

export const WEB_UI_THEME_COLORS = {
  dark: {
    background: "000000",
    surface: "313131",
    secondary: "212121",
    text: "FFFFFF",
    textSecondary: "B0B0B0",
    border: "313131",
    track: "313131",
  },
  light: {
    background: "F2F2F2",
    surface: "FFFFFF",
    secondary: "E7E7E7",
    text: "202124",
    textSecondary: "666A70",
    border: "C8CBD0",
    track: "C8CBD0",
  },
} as const;

export function previewThemeColors(theme: unknown) {
  return String(theme || "").toLowerCase() === "light"
    ? WEB_UI_THEME_COLORS.light
    : WEB_UI_THEME_COLORS.dark;
}
