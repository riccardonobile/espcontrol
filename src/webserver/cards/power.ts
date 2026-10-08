import type { CardRegistry } from "../application/card_registry";
import { cardContractAllowInSubpage, cardContractCardLabel, cardContractDefaultConfig, cardContractHidden } from "../generated/card_contract";
import { normalizeSavedConfigPower } from "../generated/saved_config_power";
import { normalizePowerOptions } from "../model/power_card";

// Phase 1 contract registration only. Hidden until the native card/editor exist;
// no settings controls, preview, rendering, actions or subscriptions here.
export function registerPowerCardTypes(registry: CardRegistry): void {
  registry.register("power", {
    label: () => cardContractCardLabel("power"),
    allowInSubpage: () => cardContractAllowInSubpage("power"),
    hidden: () => cardContractHidden("power"),
    isAvailable: () => false,
    cardMetadata: {},
    defaultConfig: () => cardContractDefaultConfig("power"),
    normalizeConfig: config => normalizeSavedConfigPower(config, normalizePowerOptions),
  });
}
