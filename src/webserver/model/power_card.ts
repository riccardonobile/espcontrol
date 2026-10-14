import { parseRawButtonConfig } from "./card";
import { utf8ByteLength } from "./subpage";
import type { CardConfig } from "../contracts/types";
import { configOptionEnabled, configOptionValue, encodeConfigField } from "./config_primitives";

// Configuration only. Power calculation and live status remain firmware-owned.
export interface PowerCardConfigV1 {
  homeEntity: string;
  solarEntity: string;
  gridEntity: string;
  batteryEntity: string;
  batterySocEntity: string;
  invertGrid: boolean;
  invertBattery: boolean;
}

export function validPowerEntity(value: string): boolean {
  return /^sensor\.[a-z0-9_]+$/.test(value.trim());
}

export function decodePowerCardConfigV1(saved: CardConfig): PowerCardConfigV1 {
  return {
    homeEntity: saved.entity.trim(),
    solarEntity: configOptionValue(saved.options, "solar_entity").trim(),
    gridEntity: configOptionValue(saved.options, "grid_entity").trim(),
    batteryEntity: configOptionValue(saved.options, "battery_entity").trim(),
    batterySocEntity: configOptionValue(saved.options, "battery_soc_entity").trim(),
    invertGrid: configOptionEnabled(saved.options, "invert_grid"),
    invertBattery: configOptionEnabled(saved.options, "invert_battery"),
  };
}

export function normalizePowerOptions(options: string): string {
  const out: string[] = [];
  for (const name of ["solar_entity", "grid_entity", "battery_entity", "battery_soc_entity"]) {
    const value = configOptionValue(options, name).trim();
    if (value) out.push(name + "=" + encodeConfigField(value));
  }
  for (const name of ["invert_grid", "invert_battery"]) {
    if (configOptionEnabled(options, name)) out.push(name);
  }
  return out.join(",");
}

/** Restore preserves missing readings in old Power cards, but never invalid IDs. */
export function powerCardConfigError(card: CardConfig, requireComplete = true): string | null {
  if (card.type !== "power") return null;
  const config = decodePowerCardConfigV1(card);
  for (const [label, value, required] of [
    ["Home power", config.homeEntity, requireComplete],
    ["Solar power", config.solarEntity, requireComplete],
    ["Grid power", config.gridEntity, requireComplete],
    ["Battery power", config.batteryEntity, requireComplete],
    ["Battery SOC", config.batterySocEntity, false],
  ] as const) {
    if ((required || value) && !validPowerEntity(value)) return "Choose a sensor entity (sensor.*) for " + label + ".";
  }
  if (String(card.options || "").split(",").some(option => /^invert_(grid|battery)=/.test(option)))
    return "Power polarity options must be flags, not values.";
  return null;
}

export function powerStoredConfigError(value: string, isSubpage = false): string | null {
  const card = parseRawButtonConfig(value);
  if (card.type !== "power") return null;
  if (!isSubpage && utf8ByteLength(value) > 255)
    return "Power settings exceed the 255-byte main-grid limit. Shorten entity IDs or the label, omit optional SOC, or use a subpage.";
  return powerCardConfigError(card, false);
}
