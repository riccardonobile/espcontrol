"use strict";

const { test } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const { loadTypescriptTest } = require("./helpers/load_typescript_test");

test("theme settings normalize, migrate, and select preview palettes", () => {
  const { runThemeSettingsTests } = loadTypescriptTest("tests/web/theme_settings.test.ts");
  runThemeSettingsTests();
});

test("media preview resolves its theme palette before every mode branch", () => {
  const source = fs.readFileSync("src/webserver/cards/media.ts", "utf8");
  const palette = source.indexOf("var themeColors: any = previewThemeColors(state.activeTheme);");
  assert.ok(palette >= 0);
  for (const mode of ["position", "cover_art", "now_playing"]) {
    const branch = source.indexOf(`if (mode === "${mode}")`, palette);
    assert.ok(branch > palette,
      `${mode} must receive the shared preview palette`);
  }
});
