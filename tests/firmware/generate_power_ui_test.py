"""Compile production Power UI/driver and shared layout with bounded LVGL doubles."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
headers = args.root / 'components/espcontrol'
layout_source = (headers / 'button_grid_layout.h').read_text(encoding='utf-8')
layout = layout_source[layout_source.index('inline void configure_button_label_wrap('):]
clock = (headers / 'clock_bar.h').read_text(encoding='utf-8')
formatter = clock[clock.index('inline void format_fixed_decimal('):clock.index('inline void format_fixed_decimal_unit(')]
navigation = (headers / 'button_grid_navigation_driver.h').read_text(encoding='utf-8')
def function(name, source=navigation):
    match = re.search(r'^inline [^\n]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Missing production function: ' + name)
    start = match.start()
    brace = match.end() - 1
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
nav_helpers = '\n'.join(function(name) for name in (
    'navigation_driver_matches', 'navigation_driver_refresh_layout', 'navigation_driver_handle_main_click'))
contrast = '\n'.join(function(name, layout_source) for name in (
    'apply_button_colors', 'apply_card_descendant_text_color', 'sync_card_checked_text_color',
    'card_text_contrast_event_cb', 'bind_card_text_contrast_events'))
sensor = function('create_card_sensor_container', (headers / 'button_grid_subpages.h').read_text(encoding='utf-8'))
theme_source = (headers / 'theme_runtime_ui.h').read_text(encoding='utf-8')
theme_helpers = '\n'.join(function(name, theme_source) for name in ('theme_grid_state_text_color', 'theme_apply_grid_button'))

template = Path(__file__).with_name('power_ui_test.cpp.in').read_text(encoding='utf-8')
args.output.write_text(template.replace('// @production_helpers', 'inline void bind_card_text_contrast_events(lv_obj_t *);\n' + formatter + contrast + layout + sensor + theme_helpers).replace('// @navigation_helpers', 'namespace espcontrol::cards {\n' + nav_helpers + '\n}'), encoding='utf-8')
