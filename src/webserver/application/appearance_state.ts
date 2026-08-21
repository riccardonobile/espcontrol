import { state } from "../state/app_instance";
import { WEB_UI_COLORS } from "../state/ui_tokens";
import {
    normalizeActiveTheme,
    normalizeThemeAutoStrategy,
    normalizeThemeMode,
    normalizeTimeOfDay,
} from "../model/settings";
import type { UiRuntimeState } from "./state";

export interface AppearanceFeature {
    syncColorUi(): void;
    syncThemeUi(): void;
    resetColors(postChanges?: boolean): void;
}

export function createAppearanceFeature(
    runtime: UiRuntimeState,
    dependencies: {
        renderPreview(): void;
        postOnColor(value: string): void;
    },
): AppearanceFeature {
    const els = runtime.els;
    // ── Appearance State ───────────────────────────────────────────────────
    function syncColorUi() {
        if (els.setOnColor && els.setOnColor._syncColor)
            els.setOnColor._syncColor(state.onColor);
    }
    function resetColors(postChanges?: boolean) {
        state.onColor = WEB_UI_COLORS.primary;
        syncColorUi();
        dependencies.renderPreview();
        if (postChanges) {
            dependencies.postOnColor(state.onColor);
        }
    }
    function syncThemeUi() {
        state.themeMode = normalizeThemeMode(state.themeMode);
        state.themeAutoStrategy = normalizeThemeAutoStrategy(state.themeAutoStrategy);
        state.themeLightStart = normalizeTimeOfDay(state.themeLightStart, "06:00");
        state.themeDarkStart = normalizeTimeOfDay(state.themeDarkStart, "18:00");
        state.activeTheme = normalizeActiveTheme(state.activeTheme);
        if (els.setThemeModeButtons) {
            for (var mode in els.setThemeModeButtons) {
                els.setThemeModeButtons[mode].classList.toggle("active", mode === state.themeMode);
            }
        }
        if (els.setThemeAutoFields) {
            els.setThemeAutoFields.className =
                "sp-cond-field" + (state.themeMode === "auto" ? " sp-visible" : "");
        }
        if (els.setThemeAutoStrategyButtons) {
            for (var strategy in els.setThemeAutoStrategyButtons) {
                els.setThemeAutoStrategyButtons[strategy].classList.toggle(
                    "active", strategy === state.themeAutoStrategy);
            }
        }
        if (els.setThemeTimeFields) {
            els.setThemeTimeFields.className = "sp-cond-field" +
                (state.themeMode === "auto" && state.themeAutoStrategy === "time" ? " sp-visible" : "");
        }
        if (els.setThemeSunFields) {
            els.setThemeSunFields.className = "sp-cond-field" +
                (state.themeMode === "auto" && state.themeAutoStrategy === "sunrise_sunset" ? " sp-visible" : "");
        }
        if (els.setThemeLightStart) els.setThemeLightStart.value = state.themeLightStart;
        if (els.setThemeDarkStart) els.setThemeDarkStart.value = state.themeDarkStart;
        if (els.setActiveTheme) els.setActiveTheme.textContent =
            "Active theme: " + (state.activeTheme === "light" ? "Light" : "Dark");
        if (els.setThemeSunInfo) {
            els.setThemeSunInfo.textContent = state.sunrise && state.sunset
                ? "Light from " + state.sunrise + " until " + state.sunset
                : "Sunrise and sunset are not currently available; the device will use Dark.";
        }
    }
    return {
        syncColorUi,
        syncThemeUi,
        resetColors,
    };
}
