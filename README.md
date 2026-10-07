# ha-bosch-optiflow

An ESPHome package that reads a **Bosch Optiflow** instantaneous hot water heater **with a Bluetooth adapter** over Bluetooth LE and exposes it to Home Assistant: temperatures, flow, burner/fan power, flame, faults, operating hours, per-use and lifetime water and gas volumes, plus setting the target temperature.

It talks the heater's "PowerBus" protocol over BLE, which I reverse-engineered by watching what the official app does. Nothing here is endorsed by or affiliated with Bosch. **Use at your own risk**: it only sends the reads the official app sends, plus the setpoint write, but it is unofficial.

## Compatibility

- Applies to Bosch Optiflow hot water units that have a **Bluetooth adapter** (the one the official Bosch app connects to). Units without it can't be used.
- **Confirmed only with models sold in Australia.** Models sold in other countries may use different parameters or firmware and are untested; the heater reports a country variant, which this package exposes. Reports from other regions are welcome.
- Tested on a single LPG unit, with an ESP32 running ESPHome 2026.9 (esp-idf framework).

Issues and PRs welcome.

## Install

1. Find the heater's Bluetooth MAC address and the pairing PIN printed on it.
2. Copy [`examples/hot-water-monitor.yaml`](examples/hot-water-monitor.yaml), set `bosch_mac` and `bosch_pin`, and flash an ESP32 near the heater.
3. The first connection pairs automatically (the heater asks for the PIN as a passkey). Both sides remember it afterwards.

The heater accepts **one BLE connection at a time**, and stops advertising while it has one. Close the Bosch app and make sure no other ESP32 is connected to it.

The package pulls the C++ helpers from this repo as an ESPHome external component; pin it with `bosch_ref: v1.0.0` in `substitutions` if you want to avoid `main` changing underneath you.

## Entities

Sensors: target / inlet / outlet / heat-exchanger / cabinet temperature, flow, burner power, fan power and speed, air flow, air and gas setpoints, last use (water, gas, duration), lifetime water volume and gas, operating hours and cycles.
Binary sensors: running, flame, fault, bathroom controller active.
Text sensors: active/last fault, fault history, recent uses, operation mode, gas type, country variant, capacity, installation, appliance type, setpoint limit, unit, firmware (main and safety).
Controls: set temperature, disconnect, reconnect.

The two volume totals are counted from the heater's own use history, so they keep running across reboots and are suitable for the Energy dashboard. A use list that arrives corrupted over BLE (a use of over 1500 L or 12 hours) is ignored rather than counted.

### "Last Shower" sensors (optional)

If your device config calls the `bosch_shower_started` / `bosch_shower_stopped` scripts (for example from a flow meter or occupancy sensor), the package splits the heater's hot water use into "shower" and "other" and publishes **Last Shower Water / Gas / Duration**. A restart within 2 minutes counts as the same shower.

## Protocol notes

See the header comment of [`packages/bosch-optiflow.yaml`](packages/bosch-optiflow.yaml) and [`components/bosch_optiflow/bosch_optiflow.h`](components/bosch_optiflow/bosch_optiflow.h):

- Frames on characteristic `EDB2`: `7B 7B | sender receiver msg_id command data... | checksum | 7D 7D`, checksum = sum of the bytes between the markers mod 256, with byte stuffing (a `7B`/`7D` inside the frame is followed by `00`).
- Every byte is XORed with the ASCII key `1ag3.-fs7`, repeating and restarting at the start of each 20-byte BLE packet.
- Login is two steps (`0xA2`): "connect", then the PIN digits.
- Commands: `0xA0` status broadcast (~4 s), `0xD2` read parameter, `0xD3` write parameter, `0xD7` fault history, `0xD8` firmware version, `0xB0` usage history.

## Licence

MIT, see [LICENSE](LICENSE).
