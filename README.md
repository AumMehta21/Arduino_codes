# Arduino Password Manager System

An embedded password manager system built using Arduino, demonstrating
hardware–software co-design with persistent backend storage.

This project focuses on system integration rather than basic Arduino
peripherals, bridging embedded systems with software and database layers.

---

## 🔧 Tech Stack
- Arduino UNO / MEGA
- TFT Display (UI)
- IR Remote (User Input)
- Python (Serial Communication & Middleware)
- MySQL (Persistent Storage)

---

## 🧠 System Architecture
---

## ⚙️ How It Works
- User interacts with the system using an IR remote.
- Arduino handles UI rendering and input decoding.
- Data is transmitted to a Python script over serial communication.
- Python processes requests and performs database operations.
- Passwords are organized by user and account labels.

---

## 📂 Repository Structure
- `sketch_dec11b.ino` – Main Arduino application
- `serial_to_mysql.py` – Python serial-to-database middleware
- `database_Linked_in.txt` – Database schema documentation


---

## 🔒 Security Note
Passwords are stored in plaintext for prototyping purposes only.
Future improvements include hashing, encryption, and hardware-based security.

---

## 🚀 Future Improvements
- Password hashing and encryption
- Hardware secure storage (EEPROM / Secure Element)
- Multi-user authentication
- RTL-level implementation of control logic (Verilog)

---

## 📌 Author
Aum Mehta
