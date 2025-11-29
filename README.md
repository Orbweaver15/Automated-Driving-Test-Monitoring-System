# Automated Driving Test System (FYP)

## 📌 Overview
This project is a sensor-based driver assessment and training platform designed to automate vehicle testing procedures. It integrates a physical steering rig with a web-based digital interface to monitor driver behavior, steering accuracy, and hand placement in real-time.

The system features a dual-mode operation: a **Test Mode** for validating driver skills against automated scenarios and a **Training Mode** (Racing Game) to improve reaction times via gamification.

## 🚀 Key Features

### 1. Real-Time Telemetry & Monitoring
* **Steering Analysis:** Utilizes the **AS5600 Magnetic Encoder** to track steering angle with high precision, detecting jitter or over-steering.
* **Hand Placement Detection:** Integrated **IR Sensors (x2)** to validate proper "10-and-2" or "9-and-3" hand positioning on the wheel.
* **Live Dashboard:** A Flask-based web interface that visualizes sensor data via Serial communication.

### 2. Gamified Training Simulation
* **Interactive Racing Game:** A custom-built browser game controlled by the physical steering rig.
* **Skill Assessment:** tracks the user's ability to maintain lane discipline and reaction speed under simulated stress.

### 3. Automated Validation
* **Pass/Fail Logic:** Automated test scenarios that instantly grade the user based on sensor thresholds (e.g., failing if hands leave the sensors for >3 seconds).

## 🛠 Tech Stack & Hardware

### Hardware Layer
* **Microcontroller:** Arduino Uno / Nano
* **Angle Sensor:** AS5600 (Non-contact Magnetic Encoder) via I2C
* **Grip Sensors:** Infrared (IR) Obstacle Sensors (x2)
* **Communication:** Serial (UART) over USB

### Software Layer
* **Backend:** Python (Flask Server)
* **Frontend:** HTML5, CSS3, JavaScript (Canvas API for Game)
* **Data Handling:** PySerial (for reading Arduino telemetry)

## ⚙️ How It Works
1.  **Data Acquisition:** The Arduino polls the AS5600 for angle data and IR sensors for grip status.
2.  **Transmission:** Data is packetized and sent via Serial (9600 baud) to the host computer.
3.  **Processing:** The Python Flask server parses the serial packets.
4.  **Visualization:** The web frontend updates the gauge cluster and game character in real-time using AJAX/WebSockets.
