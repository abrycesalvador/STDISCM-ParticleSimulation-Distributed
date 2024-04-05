#include <SFML/Graphics.hpp>
#include <iostream>
#include <math.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <string>
#include <tuple>
#include <iostream>

#include "Particle.h"
#include "FPS.cpp"

#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define MAX_BUFFER_SIZE 4096

std::mutex mtx;
std::mutex particleShapes_mtx;
std::condition_variable cv;
bool readyToRender = false;
bool readyToCompute = true;
const int numThreads = std::thread::hardware_concurrency();
int currentParticle = 0;

std::vector<std::thread> clientThreads;
const int MAX_CLIENTS = 3;

sf::View explorerView(sf::FloatRect(640 - 9.5, 360 - 16.5, 33, 19));

sf::View explorerViews[3] = { sf::View(sf::FloatRect(640 - 9.5, 360 - 16.5, 33, 19)),   //client 1
                             sf::View(sf::FloatRect(100 - 9.5, 200 - 16.5, 33, 19)),   //client 2
                             sf::View(sf::FloatRect(800 - 9.5, 600 - 16.5, 33, 19)) }; //client 3

bool activeClients[3] = { false, false, false };
clock_t activeClientsTime[3] = { 0, 0, 0 };
const clock_t TIMEOUT = 2 * CLOCKS_PER_SEC; // 2 seconds

std::vector<int> clientSocketIDs = { -1, -1, -1 };

sf::Sprite sprites[3];
sf::Texture textures[3];

std::atomic<bool> quitKeyPressed(false);
void moveExplorer(float moveX, float moveY);

void receivePosition(SOCKET client_socket) {
    int bytesReceived;
    char buffer[MAX_BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        bytesReceived = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0);
        //std::cout << "Client: " << client_socket << std::endl;
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            //std::cout << "Received: " << buffer << std::endl;

            // Parse the received data directly from the buffer
            int client_num;
            float x_float;
            float y_float;
            sscanf_s(buffer, "(%d,%f,%f)", &client_num, &x_float, &y_float);

            if (client_num >= 0 && client_num < 3) {
                activeClients[client_num] = true;
                explorerViews[client_num].setCenter(x_float, y_float);
                activeClientsTime[client_num] = clock();
                clientSocketIDs[client_num] = client_socket;
            }
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by the client." << std::endl;
            break;
        }
        else {
            std::cerr << "Receive failed with error code: " << WSAGetLastError() << std::endl;
            // find client_socket in clientSocketIDs and set it to -1
            for (int i = 0; i < 3; i++) {
                if (clientSocketIDs[i] == client_socket) {
                    clientSocketIDs[i] = -1;
                    activeClients[i] = false;
					break;
				}
			}
            break;
        }
    }
}

void sendPositionThread(SOCKET client_socket, std::vector<Particle>& particles) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return readyToRender; });

        std::stringstream ss;
        for (auto& particle : particles) {
            ss << particle.serialize() << "\n"; // Adding a newline as a delimiter
        }

        std::string serializedParticles = ss.str();
        int totalSent = 0;
        while (totalSent < serializedParticles.length()) {
            int sent = send(client_socket, serializedParticles.c_str() + totalSent, serializedParticles.length() - totalSent, 0);
            if (sent == SOCKET_ERROR) {
                // Handle error, e.g., break the loop or close socket
                break;
            }
            totalSent += sent;
        }
    }
}



void acceptClients(SOCKET server_socket, std::vector<Particle>& particles) {
    while (true) {
        SOCKET client_socket;
        sockaddr_in client_address;
        int client_address_size = sizeof(client_address);
        client_socket = accept(server_socket, reinterpret_cast<SOCKADDR*>(&client_address), &client_address_size);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Error accepting connection" << std::endl;
            closesocket(server_socket);
            WSACleanup();
            continue;
        }

        // Start a new thread to handle communication with the client
        std::thread(receivePosition, client_socket).detach();
        std::thread(sendPositionThread, client_socket, std::ref(particles)).detach();
    }

}

void updateParticles(std::vector<Particle>& particles, std::vector<sf::CircleShape>& particleShapes) {
    while (true){
        {
            std::unique_lock lk(mtx);
            cv.wait(lk, [&particles] { return readyToCompute && particles.size() > 0; });
            particles.at(currentParticle).checkCollision();
            particles.at(currentParticle).updateParticlePosition();
            particleShapes.at(currentParticle).setPosition(particles.at(currentParticle).getPosX(), particles.at(currentParticle).getPosY());
            currentParticle++;
            if (currentParticle > particles.size() - 1) {
                readyToRender = true;
                readyToCompute = false;
                cv.notify_all();
            }
        }      
    }    
}


void moveExplorer(float moveX, float moveY) {
    sf::Vector2f currentCenter = explorerView.getCenter();

    if (currentCenter.x + moveX >= 1280)
        moveX = 1280 - currentCenter.x;
    else if (currentCenter.x + moveX < 0)
        moveX = -currentCenter.x;

    if (currentCenter.y + moveY >= 720)
        moveY = 720 - currentCenter.y;
    else if (currentCenter.y + moveY < 0)
        moveY = -currentCenter.y;
    sf::Vector2f newCenter = currentCenter + sf::Vector2f(moveX, moveY);
    explorerView.setCenter(newCenter);
}

int main(){

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error initializing Winsock" << std::endl;
        return 1;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Error creating socket" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5001); // Port number on which the server will listen
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    // Bind the socket to the specified IP address and port
    if (bind(server_socket, reinterpret_cast<SOCKADDR*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Error binding socket" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Error listening on socket" << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening..." << std::endl;

    std::vector<Particle> particles;
    std::vector<sf::CircleShape> particleShapes;
    int particleCount = 0;
    
    std::thread acceptClientsThread(acceptClients, server_socket, std::ref(particles));
    //std::thread clearClientsThread(clearClients);

    // Create the main window
    sf::RenderWindow mainWindow(sf::VideoMode(1280, 720), "Particle Simulator");
    mainWindow.setFramerateLimit(60);
    ImGui::SFML::Init(mainWindow);

    auto lastFPSDrawTime = std::chrono::steady_clock::now();
    const std::chrono::milliseconds timeInterval(500); // 0.5 seconds
    FPS fps;

    sf::Font font;
    if (!font.loadFromFile("OpenSans-VariableFont_wdth,wght.ttf"))
    {
        std::cout << "error";
    }

    sf::Text fpsText;
    fpsText.setFont(font);
    fpsText.setFillColor(sf::Color::Green);
    fpsText.setStyle(sf::Text::Bold | sf::Text::Underlined);


	std::vector<std::thread> threads;

	for (int i = 0; i < numThreads; ++i) {
		threads.emplace_back(updateParticles, std::ref(particles), std::ref(particleShapes));
	}

    sf::Clock deltaClock;

    // Load textures
    if (!textures[0].loadFromFile("red.png")) {
        // handle error
        return -1;
    }
    if (!textures[1].loadFromFile("green.png")) {
        // handle error
        return -1;
    }
    if (!textures[2].loadFromFile("blue.png")) {
        // handle error
        return -1;
    }
    

    // Main loop
    while (mainWindow.isOpen())
    {
        auto currentFPSTime = std::chrono::steady_clock::now();
        auto elapsedFPSTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentFPSTime - lastFPSDrawTime);

        // Process events
        sf::Event event;
        while (mainWindow.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);
            
            if (event.type == sf::Event::Closed)
                mainWindow.close();
        }

        ImGui::SFML::Update(mainWindow, deltaClock.restart());

        mainWindow.setView(mainWindow.getDefaultView());

        ImGui::SetNextWindowPos(ImVec2(0, 0));

        ImGui::Begin("Input Particle", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SeparatorText("Add Particles");

        //imgui input numbers only
        static int numberParticles = 0;
        ImGui::InputInt("Num Particles", &numberParticles);
        ImGui::Text("");
        ImGui::Text("");

        static int startX = 0;
        static int startY = 0;
        static int endX = 0;
        static int endY = 0;
        static float speed = 0;
        static float angle = 0;

        ImGui::InputInt("Start X1", &startX);
        ImGui::InputInt("Start Y1", &startY);
        ImGui::InputInt("End X1", &endX);
        ImGui::InputInt("End Y1", &endY);
        ImGui::SliderFloat("Speed 1", &speed, 0, 11);
        ImGui::InputFloat("Angle 1", &angle);

        //imgui button input
        if (ImGui::Button("Add Case 1"))
        {
            std::cout << "CASE1: Adding " << numberParticles << " particles at " << startX << ", " << startY << " with speed " << speed << " and angle " << angle << std::endl;
            float distance = sqrt(pow(endX - startX, 2) + pow(endY - startY, 2));
            float interval = 0;
            if (numberParticles == 1) interval = 0;
            else interval = distance / (numberParticles - 1);

            for (int i = 0; i < numberParticles; i++) {
                particles.push_back(Particle(particleCount, startX + interval * i, startY + interval * i, angle, speed));
                particleShapes.push_back(sf::CircleShape(1, 10));
                particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
                particleCount++;
            }

            cv.notify_all();

        }

        ImGui::Text("");
        ImGui::Text("");

        static int startX2 = 0;
        static int startY2 = 0;
        static float speed2 = 0;
        static float angleStart = 0;
        static float angleEnd = 0;

        ImGui::InputInt("Start X2", &startX2);
        ImGui::InputInt("Start Y2", &startY2);
        ImGui::SliderFloat("Speed 2", &speed2, 0, 11);
        ImGui::InputFloat("Angle Start", &angleStart);
        ImGui::InputFloat("Angle End", &angleEnd);

        //imgui button input
        if (ImGui::Button("Add Case 2"))
        {
            std::cout << "CASE2: Adding " << numberParticles << " particles at " << startX2 << ", " << startY2 << " with speed " << speed2 << " and angle " << angleStart << " to " << angleEnd << std::endl;
            float interval = 0;
            if (numberParticles > 1) interval = (angleEnd - angleStart) / (numberParticles);

            std::cout << interval;

            for (int i = 0; i < numberParticles; i++) {
                particles.push_back(Particle(particleCount, startX2, startY2, angleStart + (interval * i), speed2));
                particleShapes.push_back(sf::CircleShape(1, 10));
                particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
                particleCount++;
            }

            cv.notify_all();

        }

        ImGui::Text("");
        ImGui::Text("");

        static int startX3 = 0;
        static int startY3 = 0;
        static float angle3 = 0;
        static float speedStart = 0;
        static float speedEnd = 0;

        ImGui::InputInt("Start X3", &startX3);
        ImGui::InputInt("Start Y3", &startY3);
        ImGui::InputFloat("Angle 3", &angle3);
        ImGui::SliderFloat("Speed Start", &speedStart, 0, 12);
        ImGui::SliderFloat("Speed End", &speedEnd, 0, 12);



        //imgui button input
        if (ImGui::Button("Add Case 3"))
        {
            std::cout << "CASE3: Adding " << numberParticles << " particles at " << startX3 << ", " << startY3 << " with angle " << angle3 << " and speed " << speedStart << " to " << speedEnd << std::endl;

            float interval = 0;
            if (numberParticles > 1) {
                interval = (speedEnd - speedStart) / (numberParticles - 1);
            }


            std::cout << interval;

            for (int i = 0; i < numberParticles; i++) {
                particles.push_back(Particle(particleCount, startX3, startY3, angle3, speedStart + (interval * i)));
                particleShapes.push_back(sf::CircleShape(1, 10));
                particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
                particleCount++;
            }

            cv.notify_all();
        }

        if (ImGui::Button("Clear Balls"))
        {
            particleCount = 0;
            particles.clear();
            particleShapes.clear();
        }

        ImGui::End();

        for (int i = 0; i < 3; ++i) {
            if (activeClients[i]) {
                sprites[i].setTexture(textures[i]);
                sprites[i].setTextureRect(sf::IntRect(0, 0, 3, 3));
                sprites[i].setOrigin(sprites[i].getLocalBounds().width / 2, sprites[i].getLocalBounds().height / 2);
                sprites[i].setPosition(explorerViews[i].getCenter());
			}
        }

        sf::RectangleShape rectShapes[3];
        for (int i = 0; i < 3; ++i) {
            if (activeClients[i]) {
                rectShapes[i].setSize(sf::Vector2f(33, 19));
                rectShapes[i].setFillColor(sf::Color::Transparent);
                rectShapes[i].setOutlineColor(sf::Color::White);
                rectShapes[i].setOutlineThickness(1);
            }
        }

        // Clear the main window
        mainWindow.clear(sf::Color{ 0, 0, 0, 255 });

        sf::RectangleShape explorerArea(sf::Vector2f(1280, 720));
        explorerArea.setFillColor(sf::Color{ 128, 128, 128, 255 });
        mainWindow.draw(explorerArea);

        if (particleShapes.size() > 0) {
            std::unique_lock lock(particleShapes_mtx);
            cv.wait(lock, [] { return readyToRender; });
            for (int i = 0; i < particleShapes.size(); i++) {
                mainWindow.draw(particleShapes[i]);
            }
            readyToCompute = true;
            readyToRender = false;
            currentParticle = 0;
            cv.notify_all();
            lock.unlock();
        }

        fps.update();

        if (elapsedFPSTime >= timeInterval)
        {
            // Update last draw time
            lastFPSDrawTime = currentFPSTime;

            
            fpsText.setString(std::to_string(fps.getFPS()) + " FPS");
            fpsText.setPosition(1150, 680);
            fpsText.setCharacterSize(30);
			
        }
        mainWindow.draw(fpsText);

        ImGui::SFML::Render(mainWindow);
        
        for (int i = 0; i < 3; ++i) {
            if (activeClients[i]) {
                sf::Vector2f spritePos = sprites[i].getPosition();
                rectShapes[i].setPosition(spritePos.x - 16.5f, spritePos.y - 9.5f);
                mainWindow.draw(sprites[i]);
                mainWindow.draw(rectShapes[i]);
            }
        }

        // Display the contents of the main window
        mainWindow.display();
    }

	for (auto& thread : threads) {
		thread.join();
	}

    ImGui::SFML::Shutdown();

    closesocket(server_socket);
    WSACleanup();

    return 0;
}
