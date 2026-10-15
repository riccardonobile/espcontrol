"""Compile production Power UI/driver and shared layout with bounded LVGL doubles."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
headers = args.root / 'components/espcontrol'
layout = (headers / 'button_grid_layout.h').read_text(encoding='utf-8')
layout = layout[layout.index('inline void configure_button_label_wrap('):]
clock = (headers / 'clock_bar.h').read_text(encoding='utf-8')
formatter = clock[clock.index('inline void format_fixed_decimal('):clock.index('inline void format_fixed_decimal_unit(')]
navigation = (headers / 'button_grid_navigation_driver.h').read_text(encoding='utf-8')
def function(name):
    start = navigation.index('inline bool ' + name + '(')
    brace = navigation.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (navigation[end] == '{') - (navigation[end] == '}')
        end += 1
    return navigation[start:end]
nav_helpers = '\n'.join(function(name) for name in (
    'navigation_driver_matches', 'navigation_driver_refresh_layout', 'navigation_driver_handle_main_click'))
template = Path(__file__).with_name('power_ui_test.cpp.in').read_text(encoding='utf-8')
args.output.write_text(template.replace('// @production_helpers', formatter + layout).replace('// @navigation_helpers', 'namespace espcontrol::cards {\n' + nav_helpers + '\n}'), encoding='utf-8')
