import { cardContractDefaultConfig, cardContractHidden, cardContractSubpageTypeCode, cardContractSubpageTypeFromCode, cardRuntimeSpec } from "../../src/webserver/generated/card_contract";
import { normalizeSavedConfigPower } from "../../src/webserver/generated/saved_config_power";
import { decodePowerCardConfigV1, normalizePowerOptions } from "../../src/webserver/model/power_card";
import { parseRawButtonConfig, CARD_CONFIG_FIELDS } from "../../src/webserver/model/card";
import { createPanelConfigBackupPayload, decodePanelConfigBackupPayload, encodePanelConfig, decodePanelConfig } from "../../src/webserver/model/panel_config";
import { createCardTransferCode, parseCardTransferCode } from "../../src/webserver/model/card_transfer";
import { CARD_SIZE_SINGLE } from "../../src/webserver/model/grid";
import { parseCompactSubpageConfig, serializeCompactSubpageConfig } from "../../src/webserver/model/subpage";
import { encodeConfigField } from "../../src/webserver/model/config_primitives";
import { createCardEditorValidationController } from "../../src/webserver/features/card_editor_validation_controller";
import { createCardRegistry } from "../../src/webserver/application/card_registry";
import { registerPowerCardTypes } from "../../src/webserver/cards/power";

function equal(actual: unknown, expected: unknown): void {
  if (JSON.stringify(actual) !== JSON.stringify(expected))
    throw new Error(`expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
}

export function runPowerCardTests(): void {
  equal(CARD_CONFIG_FIELDS.length, 9);
  const defaults = cardContractDefaultConfig("power");
  equal(defaults.label, "Power Dashboard");
  equal(decodePowerCardConfigV1(defaults), {
    homeEntity: "", solarEntity: "", gridEntity: "", batteryEntity: "", batterySocEntity: "",
    invertGrid: false, invertBattery: false,
  });
  equal(cardRuntimeSpec("power")?.driver, "power");
  equal(cardRuntimeSpec("power")?.capabilities.actions, false);
  equal(cardRuntimeSpec("power")?.capabilities.subscriptions, false); // not wired yet
  equal(cardContractHidden("power"), true);
  const registry = createCardRegistry();
  registerPowerCardTypes(registry);
  equal(registry.definitions.power?.isAvailable?.(), false);
  equal(registry.definitions.power?.renderSettings, null);
  equal(registry.definitions.power?.renderPreview, null);
  equal(cardContractSubpageTypeCode("power"), "PW");
  equal(cardContractSubpageTypeFromCode("PW"), "power");
  // Existing compact codes must not move when a new card is registered.
  equal(cardContractSubpageTypeFromCode("H"), "climate");

  const encoded = "sensor.home;My Power;Auto;Auto;;;power;1;solar_entity=sensor.solar,grid_entity=sensor.grid,battery_entity=sensor.battery,battery_soc_entity=sensor.soc,invert_grid,invert_battery";
  const card = parseRawButtonConfig(encoded);
  equal(normalizeSavedConfigPower(card, normalizePowerOptions), true);
  equal(decodePowerCardConfigV1(card), {
    homeEntity: "sensor.home", solarEntity: "sensor.solar", gridEntity: "sensor.grid",
    batteryEntity: "sensor.battery", batterySocEntity: "sensor.soc", invertGrid: true, invertBattery: true,
  });
  const canonical = { ...card };
  normalizeSavedConfigPower(card, normalizePowerOptions);
  equal(card, canonical); // normalization is idempotent
  const legacyCard = parseRawButtonConfig("light.kitchen;Kitchen;Auto;Auto;;;;;");
  const unchanged = { ...legacyCard };
  equal(normalizeSavedConfigPower(legacyCard, normalizePowerOptions), false);
  equal(legacyCard, unchanged);

  const subpage = serializeCompactSubpageConfig(["1", "B"], [["PW",
    ...[card.entity, card.label, "", "", card.sensor, card.unit, card.precision, card.options].map(encodeConfigField),
  ]]);
  equal(parseCompactSubpageConfig(subpage, cardContractSubpageTypeFromCode).buttons[0],
    { type: card.type, entity: card.entity, label: card.label, icon: card.icon, icon_on: card.icon_on,
      sensor: card.sensor, unit: card.unit, precision: card.precision, options: card.options });
  const document = {
    deviceProfile: "esp32-p4-86", buttons: { 1: encoded, 2: "light.kitchen;Kitchen;Auto;Auto;;;;;" },
    subpages: { 3: subpage },
    settings: { button_order: "1,2,3" },
  };
  equal(decodePanelConfig(encodePanelConfig(document)), document);
  equal(decodePanelConfig(decodePanelConfigBackupPayload(createPanelConfigBackupPayload(encodePanelConfig(document)))), document);
  const transfer = parseCardTransferCode(createCardTransferCode(
    { device: "esp32-p4-86", firmware: "phase-1" }, [{ ...card, size: CARD_SIZE_SINGLE }],
  ));
  equal(transfer.cards[0]?.options, card.options);
  equal(transfer.cards[0]?.type, "power");
  // Native backup codec capacity is not the live main-grid text entity limit.
  const longId = "sensor." + "power_".repeat(30);
  const longCard = { ...defaults, entity: longId, options:
    ["solar_entity", "grid_entity", "battery_entity", "battery_soc_entity"].map(name => name + "=" + longId).join(",") };
  const longEncoded = CARD_CONFIG_FIELDS.map(field => longCard[field]).join(";");
  const longDocument = { ...document, buttons: { 1: longEncoded } };
  equal(decodePanelConfig(encodePanelConfig(longDocument)), longDocument);
  const validator = createCardEditorValidationController();
  const save = (length: number) => validator.validateSave({ fields: [], isSubpage: false,
    serializedConfigLength: length, imageCardCount: 0, imageCardCapacity: 6 });
  equal(save(encoded.length).valid, true);
  equal(save(longEncoded.length).reason, "config-size"); // retain 255-byte protection
}
