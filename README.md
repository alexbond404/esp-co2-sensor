# SCD30 & SCD40 Home Assistant MQTT CO2 sensors

This project contains ESP-IDF firmware for an ESP32-based device that reads CO2 concentration, temperature, and humidity from a Sensirion SCD30 or SCD40 sensor. The collected data is then published to an MQTT broker for integration with Home Assistant.

The firmware utilizes Home Assistant's MQTT discovery capabilities, which allows for the automatic detection and configuration of the sensor entities upon connection to the MQTT broker.

## Features

- Reads CO2, temperature, and humidity from SCD30 or SCD40 sensors.
- Publishes sensor data to an MQTT broker.
- Uses Home Assistant MQTT discovery for zero-conf setup.
- Supports sensor calibration features available on the SCD30 and SCD40.
- Built with the ESP-IDF framework.
