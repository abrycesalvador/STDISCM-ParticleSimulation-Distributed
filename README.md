# STDISCM-ParticleSimulation

* Andres Dalisay
* Antoinne Bryce Salvador
* Jon Llamado
* Vincent Avila

## About this project
This is a distributed version of the previous particle simulator built using C++ using the SFML and Dear ImGui libraries. This program was given to us as a task for our STDISCM Problem Set. This was developed on Visual Studio 2022 on a Windows computer.

## Features
* Add particles with specified positions, angles, and velocities on the server
* Connect to the server and explore

## Installation
1. Clone the repository.
2. Open the .sln file in Visual Studio 2022.
3. Before running the program, make sure to set the build configuration to Release and the platform to x64.
4. Run the main server project `STDISCM-ParticleSimulation-Distributed` on a device connected to a local area network.
5. Run each of the clients on a device connected to the same local area network.
6. For the main server project, set the `SERVER_IP` definition to your device's IP address.
7. For the client project, set the `SERVER_IP` definition to the IP address of the device running the main server project.
8. Run the programs on each device.
