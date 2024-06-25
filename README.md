This project is a work in progress!

The end goal is to receive Neptune R900 water meter data using an ESP32 with a radio receiver chipset.

Current intent is to use https://github.com/NorthernMan54/rtl_433_ESP along with ESPHome, likely with a variation on https://github.com/mag1024/esphome-rtl433

However, right now, rtl_433_ESP does not play nice with high-bitrate signals - see https://github.com/NorthernMan54/rtl_433_ESP/issues/87

Below is a bit more information on Neptune R900 meters that isn't covered in rtlamr or rtl_433 source code, as they depend on broadband capture of multiple channels simultaneously while a radio chipset such as CC1101 or SX127X can only monitor one channel at once, hence we need to find out the channel hopping sequence:

analyze_hopseq.py in this repository was used to look at data collected using rtl_433 with the `-M level` option to print the actual frequency of the message, with captures of at least 700 seconds on overlapping parts of the 900 MHz band.

The R900 uses 50 hopping channels, which are spaced 131.072 kHz (4x the datarate) apart, with a gap centered at 914 MHz that is nearly 2 MHz wide.  It transmits and hops once every 14 seconds, resulting in a complete cycle across all channels every 700 seconds.  The Python script contains an implementation of the hopping algorithm which is confirmed to match at least one R900 unit, along with a complete channel list (which may be slightly off in frequency due to it being sourced by reverse engineering)

Some of this information has been confirmed via https://fccid.io/P2SNTGR900IV3/Test-Report/Test-Report-DSS-1605117.pdf