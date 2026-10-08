import type { CardConfig } from "../contracts/types";
import { configOptionEnabled, configOptionValue, encodeConfigField } from "./config_primitives";

// Configuration only: the Power editor/preview and renderer are a later phase.
export interface PowerCardConfigV1 {
  homeEntity: string;
  solarEntity: string;
  gridEntity: string;
  batteryEntity: string;
  batterySocEntity: string;
  invertGrid: boolean;
  invertBattery: boolean;
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
