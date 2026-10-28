# ESPHome Neptune R900 external component using a CC1101 (initial implementation)

Required YAML options:

- `meter_id`: unsigned 32-bit meter ID. Successfully decoded packets from other IDs are ignored before publishing.
- `cs_pin`, `gdo0_pin`, `sck_pin`, `mosi_pin`, `miso_pin`: pin assignments. `gdo2` is not needed; the CC1101's GDO2 output is configured high impedance and should be left unconnected.

Optional settings:

- `frequency`: receiver frequency in Hz or an ESPHome frequency string; defaults to `912.07101MHz`.
- `invert`: invert captured signal polarity; defaults to `false`.

All output fields are optional numeric sensors. `consumption` is converted from the decoder's raw hundredths-of-a-gallon count to gallons. `nouse`, `backflow`, `leak`, and `leak_now` are published as raw integers so unusual protocol values are preserved. `rssi` is published in dBm.
