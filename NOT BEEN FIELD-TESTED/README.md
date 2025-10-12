# 📡 ESP8266-DRA818V VHF Transceiver Web Controller

An open-source project to control the **DORJI DRA818V VHF voice transceiver module** using an **ESP8266** (like a NodeMCU or ESP-01) and a web interface. This system acts as a low-cost, feature-rich controller, allowing remote configuration of key radio parameters via Wi-Fi.

This version significantly expands upon the basic controller with advanced features like full group parameter configuration, TX power control, and a frequency scanning utility.

---

## ✨ Features

This project provides a comprehensive, responsive web interface built with **Bootstrap** for configuration and real-time control.

### **Core Transceiver Control (AT+DMOSETGROUP)**

* **Frequency Tuning:** Set independent Transmit (TX) and Receive (RX) frequencies (VHF: 134-174 MHz).
* **Squelch Level:** Control the noise threshold with 8 levels (0-8).
* **Volume Level:** Adjust the audio output volume with 8 levels (1-8).
* **Channel Spacing:** Select between **12.5 KHz (Narrow)** or **25 KHz (Wide)** bandwidth.
* **CTCSS/CDCSS Sub-Tone:** Full support for configuring Continuous Tone-Coded Squelch System (CTCSS) and Continuous Digital-Coded Squelch System (CDCSS) for both TX and RX.

### **Advanced Control & Utilities**

* **Frequency Scanner:** Implement real-time frequency scanning within a user-defined range and step size (utilizes the `S+Frequency` command).
    * Automatically tunes the radio to the first signal detected.
* **TX Power Control:** Toggle the RF output power between **1W (High)** and **0.5W (Low)** via the hardware H/L pin (GPIO 16/D0).
* **Push-to-Talk (PTT):** Remote control of the PTT line.
* **Module Power (PD):** Remote power-down capability for the DRA818V module.

---

## ⚠️ WARNING & DISCLAIMER

**THIS CODE HAS NOT BEEN FIELD-TESTED FOR ALL NEW FEATURES.**

* The implementation of the **Frequency Scanner**, **CTCSS/CDCSS** configuration, and **TX Power Control** is based purely on the official **DRA818V Datasheet** (AT Commands).
* Users must verify that these commands and controls function correctly with their specific hardware setup before relying on them for critical communication.
* Always operate within the legal frequency limits and power restrictions applicable in your region.

---

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP8266 module (NodeMCU, ESP-12E, etc.)
* **Transceiver:** DORJI DRA818V VHF Module
* **Power Supply:** A robust 3.3V-4.5V power source capable of supplying up to **1A** (required for 1W TX power).
* **Antenna:** Suitable 50Ω VHF antenna.
* **Interface:** Microphone, Speaker, and Audio Amplification circuitry (not detailed in this code).

### **Wiring Diagram (Based on the provided code)**

| ESP8266 Pin | DRA818V Pin | Description |
| :---: | :---: | :--- |
| **GPIO 5 (D1)** | RXD (Module TX) | Data from Module to ESP8266 |
| **GPIO 4 (D2)** | TXD (Module RX) | Data from ESP8266 to Module |
| **GPIO 2 (D4)** | PTT | Push-to-Talk (Low = TX) |
| **GPIO 0 (D3)** | PD | Power Down (High = ON) |
| **GPIO 16 (D0)** | H/L | TX Power Select (High = 1W, Low = 0.5W) |
| **GND** | GND | Ground |
| **3.3V** | VCC | Power Supply (Ensure proper current capacity) |

---

## 🚀 Getting Started

### **1. Setup Arduino IDE**

1.  Install the **ESP8266 Board Support Package** in the Arduino IDE.
2.  Install the **ESP8266WiFi**, **ESP8266WebServer**, **ESP8266mDNS**, and **SoftwareSerial** libraries.

### **2. Configuration**

Before uploading, modify the following lines in the `ESP8266_DRA818V.ino` sketch to set up your Wi-Fi Access Point:

```cpp
const char* ssid = "HAM_NODE";      // Your desired Wi-Fi network name
const char* password = "";          // Your desired Wi-Fi network password (leave blank for no password)