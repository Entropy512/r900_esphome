```yaml
# example configuration:

sensor:
  - platform: neptune_r900_sensor
    meter_id: !secret r900_meter

    # All SPI/RF GPIOs are configured here; no compile-time pin defines needed.
    cs_pin: GPIO27
    gdo0_pin: GPIO4
    sck_pin: GPIO18
    mosi_pin: GPIO23
    miso_pin: GPIO19

    frequency: 912.07101MHz # 912.07101MHz is halfway between Neptune R900 CHs 7 & 8, should RX 4 channels for sure, maybe 6
    invert: false

    consumption:
      name: "Water Consumption"
      device_class: water
      state_class: total_increasing
      unit_of_measurement: "gal"
      accuracy_decimals: 2

    nouse:
      name: "Days Without Use"

    backflow:
      name: "Backflow Raw Value"

    leak:
      name: "Leak Day Bins"

    leak_now:
      name: "Leak Past 24 Hours"

    rssi:
      name: "R900 Signal Strength"
```
