#!/usr/bin/env python3
"""Check that the schematic, board_config.h and platformio.ini agree.

The same pin assignment is written down in three places:

  * docs/schematic/hat-screen.kicad_sch  -- what you build
  * include/board_config.h               -- what the application code uses
  * platformio.ini                       -- what TFT_eSPI is compiled with

Nothing forces them to stay in step, and a disagreement produces a board that
builds, flashes, and then draws nothing. This script reads all three and fails
if they diverge.

Usage:
    python3 docs/schematic/tools/check_pinmap.py [--netlist FILE]

Without --netlist, kicad-cli is used to export one from the schematic.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMATIC_DIR = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(SCHEMATIC_DIR))
SCHEMATIC = os.path.join(SCHEMATIC_DIR, "hat-screen.kicad_sch")
BOARD_CONFIG = os.path.join(ROOT, "include", "board_config.h")
PLATFORMIO = os.path.join(ROOT, "platformio.ini")

MCU = "A1"
PANEL = "DS1"

# net name -> (board_config.h constant, platformio -D flag, panel pin)
SIGNALS = {
    "TFT_SCK":    ("PIN_TFT_SCLK",   "TFT_SCLK", (PANEL, "SCL")),
    "TFT_MOSI":   ("PIN_TFT_MOSI",   "TFT_MOSI", (PANEL, "SDA")),
    "TFT_CS":     ("PIN_TFT_CS",     "TFT_CS",   (PANEL, "CS")),
    "TFT_DC":     ("PIN_TFT_DC",     "TFT_DC",   (PANEL, "DC")),
    "TFT_RST":    ("PIN_TFT_RST",    "TFT_RST",  (PANEL, "RES")),
    # The backlight is driven by our own LEDC code, so TFT_eSPI is deliberately
    # told it has no backlight pin -- see PINNED_FLAGS below.
    "TFT_BL":     ("PIN_TFT_BL",     None,       (PANEL, "BLK")),
    "BTN_N":      ("PIN_BUTTON",     None,       ("SW1", None)),
    "VBAT_SENSE": ("PIN_VBAT_SENSE", None,       None),
}

# Flags that must keep a specific value, whatever the schematic says.
PINNED_FLAGS = {
    "TFT_BL": -1,    # backlight is ours, TFT_eSPI must not claim the pin
    "TFT_MISO": -1,  # the panel is never read back
}

# Rails that both the MCU and the panel must sit on.
SHARED_RAILS = ["+3V3", "GND"]

failures = []
notes = []


def fail(msg):
    failures.append(msg)


# ---------------------------------------------------------------------------
# the firmware's two copies of the pin map
# ---------------------------------------------------------------------------
def read_board_config(path):
    src = open(path).read()
    pins = {m.group(1): int(m.group(2)) for m in re.finditer(
        r"constexpr\s+int8_t\s+(\w+)\s*=\s*(-?\d+)\s*;", src)}
    floats = {m.group(1): float(m.group(2)) for m in re.finditer(
        r"constexpr\s+float\s+(\w+)\s*=\s*(-?[\d.]+)f?\s*;", src)}
    return pins, floats


def read_platformio(path):
    src = open(path).read()
    return {m.group(1): int(m.group(2)) for m in re.finditer(
        r"^\s*-D\s+(TFT_\w+)\s*=\s*(-?\d+)\s*$", src, re.M)}


# ---------------------------------------------------------------------------
# the schematic
# ---------------------------------------------------------------------------
def export_netlist(schematic):
    if not shutil.which("kicad-cli"):
        sys.exit("kicad-cli not found. Install KiCad, or pass a netlist "
                 "exported from Eeschema with --netlist.")
    tmp = tempfile.mkdtemp(prefix="hat-screen-netlist-")
    out = os.path.join(tmp, "netlist.net")
    subprocess.run(["kicad-cli", "sch", "export", "netlist",
                    "--format", "kicadsexpr", "-o", out, schematic],
                   check=True, stdout=subprocess.DEVNULL)
    return out


def read_netlist(path):
    src = open(path).read()

    values = {}
    for block in src.split("(comp (ref ")[1:]:
        ref = re.match(r'"([^"]+)"', block).group(1)
        value = re.search(r'\(value "([^"]*)"\)', block)
        values[ref] = value.group(1) if value else ""

    nets = {}
    for block in src[src.index("(nets"):].split("(net (code")[1:]:
        name = re.search(r'\(name "([^"]*)"\)', block).group(1).lstrip("/")
        nodes = re.findall(
            r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)'
            r'(?: \(pinfunction "([^"]*)"\))?', block)
        nets[name] = [(ref, pin, fn or "") for ref, pin, fn in nodes]
    return values, nets


def gpio_on(nodes, ref):
    """The GPIO number the given part contributes to a net, if any."""
    for node_ref, _pin, fn in nodes:
        if node_ref == ref:
            m = re.search(r"GPIO(\d+)", fn)
            if m:
                return int(m.group(1))
    return None


def ohms(value):
    m = re.fullmatch(r"([\d.]+)\s*([kKmMrR]?)", value.strip())
    if not m:
        return None
    scale = {"": 1, "r": 1, "R": 1, "k": 1e3, "K": 1e3, "m": 1e6, "M": 1e6}
    return float(m.group(1)) * scale[m.group(2)]


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--netlist", help="a netlist exported from the schematic; "
                                      "exported with kicad-cli if omitted")
    args = ap.parse_args()

    pins, floats = read_board_config(BOARD_CONFIG)
    flags = read_platformio(PLATFORMIO)
    values, nets = read_netlist(args.netlist or export_netlist(SCHEMATIC))

    print(f"{'NET':<12} {'SCHEMATIC':<11} {'board_config.h':<28} "
          f"{'platformio.ini':<20}")
    print("-" * 71)

    for net, (constant, flag, peer) in SIGNALS.items():
        nodes = nets.get(net)
        if nodes is None:
            fail(f"{net}: no such net in the schematic (is the label spelt "
                 f"the same on both ends?)")
            continue

        gpio = gpio_on(nodes, MCU)
        if gpio is None:
            fail(f"{net}: not connected to any {MCU} pin in the schematic")
            continue

        want = pins.get(constant)
        if want is None:
            fail(f"{net}: {constant} is not defined in board_config.h")
        elif want != gpio:
            fail(f"{net}: schematic has {MCU} on GPIO{gpio}, but "
                 f"{constant} = {want} in board_config.h")

        got_flag = flags.get(flag) if flag else None
        if flag:
            if got_flag is None:
                fail(f"{net}: platformio.ini has no -D {flag}")
            elif got_flag != gpio:
                fail(f"{net}: schematic has {MCU} on GPIO{gpio}, but "
                     f"-D {flag}={got_flag} in platformio.ini")

        if peer:
            peer_ref, peer_pin = peer
            peer_nodes = [(r, p, f) for r, p, f in nodes if r == peer_ref]
            if not peer_nodes:
                fail(f"{net}: does not reach {peer_ref} in the schematic")
            elif peer_pin and not any(f == peer_pin for _, _, f in peer_nodes):
                fail(f"{net}: reaches {peer_ref} but not its {peer_pin} pin "
                     f"(found {', '.join(f for _, _, f in peer_nodes)})")

        print(f"{net:<12} {'GPIO' + str(gpio):<11} "
              f"{constant + ' = ' + str(want):<28} "
              f"{(flag + ' = ' + str(got_flag)) if flag else '-':<20}")

    print()

    for flag, want in PINNED_FLAGS.items():
        got = flags.get(flag)
        if got != want:
            fail(f"platformio.ini: -D {flag} must be {want}, found {got}")
        else:
            notes.append(f"-D {flag}={want} as expected")

    for rail in SHARED_RAILS:
        nodes = nets.get(rail, [])
        refs = {r for r, _, _ in nodes}
        missing = [r for r in (MCU, PANEL) if r not in refs]
        if missing:
            fail(f"{rail}: does not reach {', '.join(missing)}")
        else:
            notes.append(f"{rail} is common to {MCU} and {PANEL}")

    # The firmware scales the ADC reading by a fixed ratio; the divider on the
    # schematic has to actually produce it.
    r_top, r_bottom = ohms(values.get("R1", "")), ohms(values.get("R2", ""))
    want_ratio = floats.get("VBAT_DIVIDER_RATIO")
    if r_top is None or r_bottom is None:
        fail("battery divider: cannot read R1/R2 values from the schematic")
    elif want_ratio is None:
        fail("battery divider: VBAT_DIVIDER_RATIO is not defined in "
             "board_config.h")
    else:
        ratio = (r_top + r_bottom) / r_bottom
        if abs(ratio - want_ratio) > 0.01:
            fail(f"battery divider: R1={values['R1']} R2={values['R2']} "
                 f"divides by {ratio:.3f}, but VBAT_DIVIDER_RATIO = "
                 f"{want_ratio}")
        else:
            notes.append(f"battery divider R1/R2 gives {ratio:.2f}, matching "
                         f"VBAT_DIVIDER_RATIO")

    for note in notes:
        print(f"  ok   {note}")

    if failures:
        print()
        for f in failures:
            print(f"  FAIL {f}", file=sys.stderr)
        print(f"\n{len(failures)} mismatch(es) between the schematic and the "
              f"firmware.", file=sys.stderr)
        return 1

    print("\nSchematic, board_config.h and platformio.ini agree.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
