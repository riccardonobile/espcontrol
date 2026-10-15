import type { CardRegistry } from "../application/card_registry";
import { cardContractAllowInSubpage, cardContractCardLabel, cardContractDefaultConfig, cardContractHidden, cardContractOptions } from "../generated/card_contract";
import { sizeColSpan } from "../model/grid";
import { normalizeSavedConfigPower } from "../generated/saved_config_power";
import { decodePowerCardConfigV1, normalizePowerOptions, validPowerEntity } from "../model/power_card";
import { setConfigOption, setConfigOptionValue } from "../model/config_primitives";
import { utf8ByteLength } from "../model/subpage";
import type { ControlsFieldsFeature } from "../application/controls_fields";

export function registerPowerCardTypes(registry: CardRegistry, fields: Pick<ControlsFieldsFeature, "cardSensorPreviewHtml">): void {
  registry.register("power", {
    label: () => cardContractCardLabel("power"),
    allowInSubpage: () => cardContractAllowInSubpage("power"),
    hidden: () => cardContractHidden("power"),
    cardMetadata: {},
    labelPlaceholder: "Power Dashboard",
    defaultConfig: () => cardContractDefaultConfig("power"),
    normalizeConfig: config => normalizeSavedConfigPower(config, normalizePowerOptions),
    onSelect: config => normalizeSavedConfigPower(config, normalizePowerOptions),
    renderSettings: (panel, card, _slot, helpers) => {
      const config = decodePowerCardConfigV1(card);
      const optionLabel = (name: string) => cardContractOptions("power").find(option => option.name === name)!.label;
      const sensors = [
        { key: "entity", label: "Home power", value: config.homeEntity, note: "Positive consumption. Use an instantaneous power sensor reporting W or kW.", required: true },
        { key: "solar_entity", label: optionLabel("solar_entity"), value: config.solarEntity, note: "Positive production. Required for complete inferred flows; W or kW.", required: true },
        { key: "grid_entity", label: optionLabel("grid_entity"), value: config.gridEntity, note: "Positive importing, negative exporting; W or kW.", required: true },
        { key: "battery_entity", label: optionLabel("battery_entity"), value: config.batteryEntity, note: "Positive discharging, negative charging; W or kW.", required: true },
        { key: "battery_soc_entity", label: optionLabel("battery_soc_entity") + " (optional)", value: config.batterySocEntity, note: "A separate percentage sensor (0–100%). Does not affect power flows.", required: false },
      ];
      for (const sensor of sensors) {
        const field = helpers.entityField(sensor.label, helpers.idPrefix + "power-" + sensor.key,
          sensor.value, "sensor." + (sensor.key === "entity" ? "home_power" : sensor.key.replace("_entity", "")),
          ["sensor"], null, false);
        panel.appendChild(field.field);
        if (sensor.key === "entity") helpers.markCardPrimaryField(field.field, "entity");
        helpers.requireField(field.input, "Choose a sensor entity (sensor.*) for " + sensor.label + ".", undefined,
          (value: string) => (!sensor.required && !value.trim()) || validPowerEntity(value));
        const sync = () => {
          const value = field.input.value.trim();
          if (sensor.key === "entity") card.entity = value;
          else card.options = normalizePowerOptions(setConfigOptionValue(card.options, sensor.key, value));
          helpers.saveField(sensor.key === "entity" ? "entity" : "options", sensor.key === "entity" ? card.entity : card.options);
        };
        field.input.addEventListener("input", sync);
        field.input.addEventListener("change", sync);
        const note = document.createElement("div");
        note.className = "sp-apply-note";
        note.textContent = sensor.note;
        field.field.appendChild(note);
      }
      for (const name of ["invert_grid", "invert_battery"] as const) {
        const toggle = helpers.toggleRow(optionLabel(name), helpers.idPrefix + "power-" + name,
          name === "invert_grid" ? config.invertGrid : config.invertBattery);
        panel.appendChild(toggle.row);
        toggle.input.addEventListener("change", () => {
          card.options = normalizePowerOptions(setConfigOption(card.options, name, toggle.input.checked));
          helpers.saveField("options", card.options);
        });
      }
      const precision = helpers.precisionField(helpers.idPrefix + "power-precision", card.precision || "0", function (this: HTMLSelectElement) {
        card.precision = this.value === "0" ? "" : this.value;
        helpers.saveField("precision", card.precision);
      });
      panel.appendChild(precision.field);
      const note = document.createElement("div");
      note.className = "sp-apply-note";
      note.textContent = "Requires at least two grid columns. Precision is decimal places in watts (never kW). Arrows are inferred allocations from aggregate readings, not measured individual routes. Preview uses placeholders, not live Home Assistant measurements. The device shows live Home power and Battery/Grid status. The card uses your shared accent when pressed; it does not display an icon.";
      panel.appendChild(note);
      const sizeNote = document.createElement("div");
      sizeNote.className = "sp-apply-note";
      sizeNote.id = helpers.idPrefix + "power-config-size";
      sizeNote.setAttribute("role", "status");
      panel.appendChild(sizeNote);
      const updateSize = () => {
        const bytes = utf8ByteLength(helpers.serializeCardConfig(card));
        sizeNote.textContent = helpers.isSub
          ? "Subpage storage is shared across its cards; Save checks the complete chunked configuration."
          : "Main-grid configuration: " + bytes + " / 255 bytes." + (bytes > 255
            ? " Too large: shorten entity IDs or the label, omit optional SOC, or use a subpage. Nothing will be truncated." : "");
      };
      panel.addEventListener("input", updateSize);
      panel.addEventListener("change", updateSize);
      updateSize();
    },
    renderPreview: (card, helpers) => {
      const narrow = sizeColSpan(helpers.cardSize) < 2;
      const label = '<span class="sp-btn-label">' + helpers.escHtml(card.label || "Power Dashboard") + '</span>';
      const arrow = '<span class="sp-navigation-chevron sp-power-chevron mdi mdi-chevron-right" aria-hidden="true"></span>';
      return {
        buttonClass: "sp-power-card" + (narrow ? " sp-power-narrow" : ""),
        iconHtml: narrow ? "" : fields.cardSensorPreviewHtml(card, helpers, "--", "W"),
        labelHtml: narrow ? '<span class="sp-btn-label-row">' + label + arrow + '</span>'
          : '<span class="sp-power-labels">' + label + '<span class="sp-btn-label-row"><span class="sp-btn-label sp-power-secondary">Unavailable</span>' + arrow + '</span></span>',
      };
    },
  });
}
