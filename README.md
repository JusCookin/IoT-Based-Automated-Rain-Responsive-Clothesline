# IoT-Based-Automated-Rain-Responsive-Clothesline
An automated cloth protection system that detects rain and shifts clothes to a covered area. It includes real-time monitoring, OLED display, Google Sheets logging, and a web page for live data display.
ESP32 Automatic Cloth Protection System
An intelligent IoT solution that automatically protects drying clothes from rain using ESP32, with real-time monitoring and Google Sheets data logging.

🌧️ Overview
This project creates an automated cloth protection system that monitors weather conditions and mechanically moves clothes to a covered area when rain is detected. The system features real-time monitoring, OLED display feedback, cloud-based data logging through Google Sheets integration, and a frontend web page that displays real-time data from Google Sheets.
✨ Features

Automatic Rain Detection: Uses MH-RD rain sensor for precise moisture detection
Servo-Controlled Protection: SG90 servo motor moves clothes between outdoor and covered positions
Real-time Environmental Monitoring: DHT11 sensor tracks temperature and humidity
Visual Feedback: 128x64 OLED display shows system status and sensor readings
Cloud Data Logging: Automatic data upload to Google Sheets for analysis
Web Dashboard: Frontend web page displays real-time sensor data from Google Sheets
WiFi Connectivity: Remote monitoring and data transmission capabilities

🔧 Hardware Components

ESP32 Development Board - Main microcontroller
MH-RD Rain Sensor - Detects moisture/rain
SG90 Servo Motor - Moves clothes mechanism
DHT11 Sensor - Temperature and humidity monitoring
128x64 OLED Display - System status display

📊 System Components

ESP32 IoT Device: Monitors sensors and controls servo motor
Google Sheets Integration: Cloud-based data storage and logging
Frontend Web Dashboard: Real-time data visualization from Google Sheets
OLED Display: Local system status and sensor readings

Hardware Architecture:
![image](https://github.com/user-attachments/assets/f159b393-e180-4288-b5cc-c8c8527655c7)

Interface:
![WhatsApp Image 2025-06-16 at 19 20 43_3f31fe05](https://github.com/user-attachments/assets/8fc2c515-d2b8-4447-866a-5ffc3801c510)

