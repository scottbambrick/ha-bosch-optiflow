#pragma once
// Bosch Optiflow / PowerBus-over-BLE helpers for packages/bosch-optiflow.yaml.
// The protocol summary is at the top of that file.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Every byte on the wire is XORed with this key, repeating, restarting at the
// start of each 20-byte BLE packet.
static const uint8_t BOSCH_KEY[9] = {'1', 'a', 'g', '3', '.', '-', 'f', 's', '7'};

// Two-digit uppercase hex, e.g. 0xC5 -> "C5".
inline std::string bosch_hex2(uint8_t v) {
  char s[4];
  snprintf(s, sizeof(s), "%02X", v);
  return std::string(s);
}

// Space-separated hex dump, for logging.
inline std::string bosch_hex(const std::vector<uint8_t> &bytes) {
  std::string out;
  for (uint8_t b : bytes) out += bosch_hex2(b) + " ";
  return out;
}

// Builds one request frame (sent from the app address, 0x21). Login (0xA2)
// goes to the login handler (0xAB); everything else to the heater (0x00).
inline std::vector<uint8_t> bosch_encode(uint8_t msg_id, uint8_t cmd, const std::vector<int> &payload) {
  uint8_t to = (cmd == 0xA2) ? 0xAB : 0x00;
  std::vector<uint8_t> body = {0x21, to, msg_id, cmd};
  for (int p : payload) body.push_back((uint8_t) p);
  uint8_t cs = 0;
  for (uint8_t b : body) cs += b;
  body.push_back(cs);
  std::vector<uint8_t> frame = {0x7B, 0x7B};
  frame.reserve(body.size() + 8);
  for (uint8_t b : body) {
    frame.push_back(b);
    if (b == 0x7B || b == 0x7D) frame.push_back(0x00);  // byte stuffing
  }
  frame.push_back(0x7D);
  frame.push_back(0x7D);
  for (size_t j = 0; j < frame.size(); j++) frame[j] ^= BOSCH_KEY[(j % 20) % 9];
  return frame;
}

enum BoschRx { BOSCH_RX_INCOMPLETE, BOSCH_RX_BAD_CHECKSUM, BOSCH_RX_OK };

// Feeds one BLE notification into the frame buffer `buf`. When a complete
// frame has arrived, un-stuffs it into `body` (sender, receiver, msg id,
// command, data..., checksum) and verifies the checksum.
inline BoschRx bosch_receive(const std::vector<uint8_t> &x, std::vector<uint8_t> &buf,
                             std::vector<uint8_t> &body) {
  // 1) Undo the XOR (the key restarts at every BLE packet).
  std::vector<uint8_t> chunk(x.size());
  for (size_t j = 0; j < x.size(); j++) chunk[j] = x[j] ^ BOSCH_KEY[j % 9];

  // 2) Reassemble: frames start 7B 7B and end 7D 7D.
  if (chunk.size() >= 2 && chunk[0] == 0x7B && chunk[1] == 0x7B) buf.clear();
  buf.insert(buf.end(), chunk.begin(), chunk.end());
  if (buf.size() < 2 || buf[0] != 0x7B || buf[1] != 0x7B) {
    buf.clear();
    return BOSCH_RX_INCOMPLETE;
  }
  size_t n = buf.size();
  if (n < 6 || buf[n - 2] != 0x7D || buf[n - 1] != 0x7D) {
    if (n > 512) buf.clear();
    return BOSCH_RX_INCOMPLETE;  // frame not complete yet
  }

  // 3) Strip markers and undo byte stuffing.
  body.clear();
  for (size_t i = 2; i < n - 2; i++) {
    body.push_back(buf[i]);
    if ((buf[i] == 0x7B || buf[i] == 0x7D) && i + 1 < n - 2 && buf[i + 1] == 0x00) i++;
  }
  buf.clear();
  if (body.size() < 5) return BOSCH_RX_INCOMPLETE;

  // 4) Verify the checksum.
  uint8_t cs = 0;
  for (size_t i = 0; i + 1 < body.size(); i++) cs += body[i];
  return cs == body.back() ? BOSCH_RX_OK : BOSCH_RX_BAD_CHECKSUM;
}

// Fault descriptions, verbatim from the Bosch app's strings.
// (The app swaps the A7 and A8 texts for PowerBus heaters; so do we.)
inline const char *bosch_fault_description(uint8_t code) {
  switch (code) {
    case 0xA0: return "Water temperature sensors (outlet hot water, heat exchanger hot water and inlet cold water temperature sensors) disconnected or damaged.";
    case 0xA1: return "Air temperature inside the cabinet is above 70°C / 158°F. Appliance regulates power to protect against overheat.";
    case 0xA2: return "External water temperature sensor/aquastat disconnected, damaged or not properly installed.";
    case 0xA3: return "Flue gases temperature sensor disconnected or damaged.";
    case 0xA4: return "Air temperature sensor disconnected or damaged.";
    case 0xA5: return "Condensing unit flue temperature sensor disconnected or damaged.";
    case 0xA6: return "Inlet cold water temperature sensor disconnected or damaged.";
    case 0xA7: return "Outlet hot water temperature sensor disconnected or damaged.";
    case 0xA8: return "Heat exchanger hot water temperature sensor disconnected or damaged.";
    case 0xA9: return "Incorrect output power (too low).";
    case 0xAA: return "Condensing unit flue gases temperature above 200°C/392°F and/or flue gases temperature above 62°C/145°F at the exhaust sensor. Appliance regulates power to protect against overheating.";
    case 0xC1: return "Flue blockage detected during startup due to insufficient combustion air.";
    case 0xC2: return "Flue blockage detected during startup due to insufficient combustion air.";
    case 0xC3: return "Flow sensor does not detect water recirculation.";
    case 0xC5: return "Bypass Valve disconnected or damaged.";
    case 0xC7: return "Fan disconnected or damaged.";
    case 0xC8: return "Main water valve disconnected or damaged.";
    case 0xC9: return "Barometric pressure sensor damaged.";
    case 0xCA: return "Water flow above maximum value specified.";
    case 0xCE: return "Flue blockage detected during operation due to excessive pressure on the siphon.";
    case 0xCF: return "Flue blockage detected during operation due to insufficient combustion air.";
    case 0xE0: return "Electronic control unit internal error.";
    case 0xE1: return "Hot water temperature above maximum allowed limit. Appliance burner cut off to prevent scalding and reactivation after cooling down.";
    case 0xE2: return "Incoming temperature sensor faulty.";
    case 0xE3: return "Flue gases temperature above 75°C/167°F (residential) / 90°C/194°F (commercial) or 2 minutes above 62°C/145°F. Appliance burner cut off to prevent overheating.";
    case 0xE4: return "Air temperature inside cabinet above 80°C/176°F or 2 minutes above 70°C/158°F. Appliance burner cut off to prevent overheating.";
    case 0xE5: return "Condensing unit flue gases temperature above 220°C/428°F or 2 minutes above 200°C/392°F. Appliance burner cut off to prevent overheating.";
    case 0xE7: return "Electronic control unit internal error.";
    case 0xE8: return "Error during ionization test. Electronic control unit internal error.";
    case 0xE9: return "Thermal fuse broken.";
    case 0xEA: return "Ignition failure.";
    case 0xEB: return "Electronic control unit internal error.";
    case 0xEC: return "Flame lost during operation.";
    case 0xEE: return "Gas valve modulation solenoid disconnected";
    case 0xEF: return "Wrong gas connected (LP instead of NG) for current appliance configuration.";
    case 0xF2: return "Electronic control unit internal error.";
    case 0xF3: return "Electronic control unit internal error.";
    case 0xF7: return "Electronic control unit internal error.";
    case 0xF8: return "Electronic control unit internal error.";
    case 0xF9: return "Gas valve driver internal fault.";
    case 0xFA: return "Gas leakage in the gas path (gas valve or gas manifold shut-off valves).";
    case 0xFC: return "Key pressed for more than 30 seconds.";
    default: return "Unknown fault code";
  }
}

// e.g. "C5: Bypass Valve disconnected or damaged.", or "No fault" for 0.
inline std::string bosch_fault_text(uint8_t code) {
  if (code == 0) return "No fault";
  return bosch_hex2(code) + ": " + bosch_fault_description(code);
}

// --- Hot water use totals ----------------------------------------------------
// The heater only reports its last few uses, newest first. The newest entry is
// written while the water is still running and grows until the use finishes.
// To build running totals, each poll's list is lined up with the previous one
// and only what's new is counted: whole new uses, plus any growth of the use
// that was newest last time.

// One use packed into a comparable key: gas << 32 | water << 16 | minutes.
inline uint64_t bosch_use_key(uint16_t gas, uint16_t water, uint16_t minutes) {
  return ((uint64_t) gas << 32) | ((uint32_t) water << 16) | minutes;
}
inline uint16_t bosch_use_gas(uint64_t key) { return (key >> 32) & 0xFFFF; }
inline uint16_t bosch_use_water(uint64_t key) { return (key >> 16) & 0xFFFF; }
inline uint16_t bosch_use_minutes(uint64_t key) { return key & 0xFFFF; }

// A list read from the heater can be corrupted (2026-10-07: "103 L / 3328 min, 21760 L / 2816
// min" - the real "103 L / 13 min" shifted by a byte), and it still lined up with the previous
// list and added ~48,000 L. A real use is under a few hundred litres and a few hours.
inline bool bosch_list_plausible(const std::vector<uint64_t> &uses) {
  for (uint64_t u : uses)
    if (bosch_use_water(u) > 1500 || bosch_use_minutes(u) > 720) return false;
  return true;
}

// True if `now` is `before`, or `before` after it grew (use still running).
inline bool bosch_use_grew(uint64_t now, uint64_t before) {
  return bosch_use_gas(now) >= bosch_use_gas(before) && bosch_use_water(now) >= bosch_use_water(before) &&
         bosch_use_minutes(now) >= bosch_use_minutes(before);
}

// Adds the gas and water (litres) that are new in `cur` compared with `prev`
// (the previous list, or the first few entries saved before a reboot).
// Returns false if the lists don't line up, in which case nothing is added.
inline bool bosch_count_new_uses(const std::vector<uint64_t> &cur, const std::vector<uint64_t> &prev,
                                 float &gas, float &water) {
  if (prev.empty() || cur.empty()) return true;  // nothing to compare with yet
  for (size_t k = 0; k < cur.size(); k++) {
    // e.g. [N, A', B, C, D] after [A, B, C, D, E]: k = 1, A' = A or A grown.
    bool match = bosch_use_grew(cur[k], prev[0]);
    for (size_t j = 1; match && k + j < cur.size() && j < prev.size(); j++) match = cur[k + j] == prev[j];
    if (!match) continue;
    for (size_t i = 0; i < k; i++) {
      gas += bosch_use_gas(cur[i]);
      water += bosch_use_water(cur[i]);
    }
    gas += bosch_use_gas(cur[k]) - bosch_use_gas(prev[0]);
    water += bosch_use_water(cur[k]) - bosch_use_water(prev[0]);
    return true;
  }
  return false;
}
