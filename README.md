### Project Proposal: ESP8266 D1 Mini and DRA818V Ham Radio Tuner

#### Project Overview

This project is designed to create a web-based tuner interface for the DRA818V ham radio module using the ESP8266 D1 Mini microcontroller. The interface allows users to adjust the frequency, squelch level, and volume of the DRA818V module through a web browser. It also provides real-time control over the PTT (Push-To-Talk) and power states of the module.

#### Components

1. **ESP8266 D1 Mini**: A powerful, low-cost Wi-Fi microcontroller that enables the web-based interface and handles communication with the DRA818V module.
2. **DRA818V**: A VHF/UHF transceiver module used for amateur radio applications, controlled via AT commands.
3. **SoftwareSerial Library**: Used for serial communication between the ESP8266 and the DRA818V module.
4. **ESP8266WebServer Library**: Provides the web server functionality to host the tuning interface.

#### Key Features

1. **Web-based Tuning Interface**: Users can adjust the frequency, squelch level, and volume settings through a user-friendly web interface.
2. **Increment/Decrement Frequency Control**: Buttons to increment or decrement the frequency based on predefined steps (2.5KHz, 5.0KHz, 6.25KHz, 10.0KHz, 12.5KHz, 20.0KHz, 25.0KHz, 50.0KHz).
3. **Real-time Control**: Immediate application of changes without needing to refresh or reload the interface.
4. **PTT and Power Control**: Toggle buttons to control the PTT and power states of the DRA818V module.
5. **Bootstrap Integration**: The web interface uses Bootstrap for a clean and responsive design.

#### Technical Details

- **Frequency Control**: Users can input a frequency or use the provided buttons to adjust the frequency by predefined steps.
- **AT Commands**: The ESP8266 sends AT commands to the DRA818V module to set the frequency, squelch level, and volume.
- **Form Submission Handling**: The web server handles form submissions and updates the module settings accordingly.
- **Real-time Feedback**: The interface provides real-time feedback on the current status of the PTT and power states.

#### Code Breakdown

- **Setup**:
  - Initializes the ESP8266 as a Wi-Fi access point.
  - Sets up the web server and defines the endpoints for handling requests.
  - Configures the SoftwareSerial for communication with the DRA818V module.

- **Web Interface**:
  - Provides a form for users to input the frequency, squelch level, and volume.
  - Includes buttons for incrementing and decrementing the frequency.
  - Contains toggle buttons for controlling the PTT and power states.

- **Form Handling**:
  - Processes the form submissions and sends the appropriate AT commands to the DRA818V module.
  - Updates the interface with the current settings and statuses.

#### Usage Instructions

1. **Hardware Setup**:
   - Connect the ESP8266 D1 Mini to the DRA818V module via the appropriate GPIO pins.
   - Ensure proper power supply and antenna connections for the DRA818V module.

2. **Software Setup**:
   - Upload the provided code to the ESP8266 D1 Mini using the Arduino IDE.
   - Connect to the Wi-Fi network broadcasted by the ESP8266 (SSID: "HAM_NODE").

3. **Using the Web Interface**:
   - Open a web browser and navigate to the IP address of the ESP8266 (default: 192.168.1.35).
   - Adjust the frequency, squelch level, and volume using the provided form.
   - Use the PTT and Power buttons to control the respective states of the DRA818V module.

#### Future Enhancements

- **Security Features**: Implement user authentication and HTTPS for secure access.
- **Expanded Frequency Control**: Allow for more granular frequency adjustments.
- **Additional Module Support**: Extend support to other radio modules and frequency bands.

This project offers a robust and user-friendly solution for remotely controlling the DRA818V ham radio module, making it an excellent addition to any amateur radio enthusiast's toolkit.
