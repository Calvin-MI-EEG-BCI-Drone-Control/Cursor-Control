# Data Collection

This directory contains all the code for running this project. 

- NOTE: I used VCPKG as a package manager, so the variable VCPKG_ROOT refers to the directory where some of these packages were installed.

### Collector.cpp
- Compile with: g++ Collector.cpp -o Collector -I"../iWorxDAQ_64" -L"../iWorxDAQ_64" -liwxDAQ -I"$VCPKG_ROOT/installed/x64-windows/include" -L"$VCPKG_ROOT/installed/x64-windows/lib" -lsqlite3 -lpaho-mqtt3cs
- Run with: ./Collector <database-name>

### DataStream.py
- Used to subscribe to the MQTT broker to collect data sent by Collector.cpp

### Predictions.py
- Used to integrate the data collection, machine learning, and cursor control. The main program.
- When running this code, run this program first THEN run Collector.cpp. A DataStream will wait for the data from the publisher, but Collector.cpp will not wait for the DataStream to be ready before sending.