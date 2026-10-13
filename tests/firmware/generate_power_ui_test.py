"""Compile production Power UI/driver and shared layout with bounded LVGL doubles."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
headers = args.root / 'components/espcontrol'
layout = (headers / 'button_grid_layout.h').read_text(encoding='utf-8')
layout = layout[layout.index('inline void configure_button_label_wrap('):layout.index('inline void set_subpage_chevron_visible(')]
clock = (headers / 'clock_bar.h').read_text(encoding='utf-8')
formatter = clock[clock.index('inline void format_fixed_decimal('):clock.index('inline void format_fixed_decimal_unit(')]
template = Path(__file__).with_name('power_ui_test.cpp.in').read_text(encoding='utf-8')
args.output.write_text(template.replace('// @production_helpers', formatter + layout), encoding='utf-8')
