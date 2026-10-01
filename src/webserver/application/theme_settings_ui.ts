import type { AppState } from "../state/types";
import type { UiRuntimeState } from "./state";

export function syncThemeSettingsUi(state: AppState, runtime: UiRuntimeState): void {
  const els = runtime.els;
  const buttons = els.setThemeModeButtons || {};
  for (const mode of ["Dark", "Light", "Schedule", "Sun"]) {
    const button = buttons[mode];
    if (button) button.classList.toggle("active", state.themeMode === mode);
  }
  if (els.setThemeScheduleFields)
    els.setThemeScheduleFields.className =
      "sp-cond-field" + (state.themeMode === "Schedule" ? " sp-visible" : "");
  if (els.setThemeLightStart) els.setThemeLightStart.value = state.themeLightStart;
  if (els.setThemeDarkStart) els.setThemeDarkStart.value = state.themeDarkStart;
}
