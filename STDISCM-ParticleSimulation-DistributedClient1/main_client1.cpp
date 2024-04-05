#include <SFML/Graphics.hpp>
#include <iostream>
#include <math.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>


//#include "Particle.h"
#include "FPS.cpp"

#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"
#include "../STDISCM-ParticleSimulation-Distributed/Particle.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define MAX_BUFFER_SIZE 4096
#define CLIENT_ID 1

std::mutex mtx;
std::condition_variable cv;
bool readyToRender = false;
bool readyToCompute = true;

std::pair<float, float> last_position = std::make_pair(0, 0);

sf::View explorerView(sf::FloatRect(640 - 9.5, 360 - 16.5, 33, 19));
void moveExplorer(float moveX, float moveY);

void sendLocation(SOCKET client_socket, sf::View& explorer) {
    while (true) {
        sf::Vector2 vec_position = explorer.getCenter();
        std::pair<float, float> position = std::make_pair(vec_position.x, vec_position.y);
        // Don't send position if it hasn't changed
        if (last_position == position) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}
        last_position = position;
        //std::cout << "Sending position: " << position.first << ", " << position.second << std::endl;

        std::ostringstream oss;
        oss << "(" << CLIENT_ID << ", " << position.first << ", " << position.second << ")";
        std::string sendString = oss.str();

        int bytes_sent = send(client_socket, sendString.c_str(), sendString.size(), 0);        
        if (bytes_sent == SOCKET_ERROR) {
            std::cerr << "Error sending data to server" << std::endl;
            break;
        }
    }
}

void receiveParticleData(SOCKET client_socket, std::map<int, sf::CircleShape>& particleShapes) {
    int bytesReceived;
    char buffer[MAX_BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, MAX_BUFFER_SIZE);
        bytesReceived = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string receivedData(buffer);
            std::istringstream iss(receivedData);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty()) {
                    Particle particle = Particle::deserialize(line);
                    sf::CircleShape particleShape(1);
                    particleShape.setOrigin(particleShape.getRadius(), particleShape.getRadius());
                    particleShape.setPosition(particle.getPosX(), particle.getPosY());
                    particleShapes[particle.getId()] = std::move(particleShape);
                }
            }
            readyToRender = true;
            cv.notify_one();
        }
        else if (bytesReceived == 0) {
            std::cout << "Connection closed by the server." << std::endl;
            break;
        }
        else {
            std::cerr << "Receive failed with error code: " << WSAGetLastError() << std::endl;
            closesocket(client_socket);
            break;
        }
    }
}


void keyboardInputListener() {
    while (true) {
        float moveX = 5, moveY = 5;   // Change values for how distance explorer moves.
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
            moveExplorer(0, -moveY);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Debounce time to avoid rapid movement
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
            moveExplorer(-moveX, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
            moveExplorer(0, moveY);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
            moveExplorer(moveX, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

SOCKET initializeSocket() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error initializing Winsock" << std::endl;
        return 1;
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Error creating socket" << std::endl;
        WSACleanup();
        return 1;
    }

    return client_socket;
}

bool connectToServer(SOCKET client_socket) {
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5001); // Port number of the server
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    if (connect(client_socket, reinterpret_cast<SOCKADDR*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Error connecting to server. Retrying in 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        //closesocket(client_socket);
        //WSACleanup();
        return false;
    }

    
    return true;
}

int main()
{
    SOCKET client_socket = initializeSocket();
    bool connected = false;
    do {
        connected = connectToServer(client_socket);
    } while (!connected);
    std::cout << "Connected to server" << std::endl;

    // Create the main window
    sf::RenderWindow mainWindow(sf::VideoMode(1280, 720), "Particle Simulator Client 1");
    mainWindow.setFramerateLimit(60);
    ImGui::SFML::Init(mainWindow);
    sf::Clock deltaClock;

    // Setup FPS
    FPS fps;
    auto lastFPSDrawTime = std::chrono::steady_clock::now();
    const std::chrono::milliseconds timeInterval(500); // 0.5 seconds
    sf::Font font;
    if (!font.loadFromFile("OpenSans-VariableFont_wdth,wght.ttf"))
    {
        std::cout << "error";
    }
    sf::Text fpsText;
    fpsText.setFont(font);
    fpsText.setFillColor(sf::Color::Green);
    fpsText.setStyle(sf::Text::Bold | sf::Text::Underlined);
    
	std::map<int, sf::CircleShape> particleShapes;

    // Utility threads
    std::thread receiveParticleDataThread(receiveParticleData, client_socket, std::ref(particleShapes));
    std::thread keyboardThread(keyboardInputListener);
    std::thread sendLocationThread(sendLocation, client_socket, std::ref(explorerView));
    
    sf::Sprite sprite;
    sf::Texture texture;
    if (!texture.loadFromFile("red.png")) {
        std::cerr << "Error loading texture" << std::endl;
        return -1;
    }
    sprite.setTexture(texture);
    sprite.setTextureRect(sf::IntRect(0, 0, 3, 3));
    sprite.setOrigin(sprite.getLocalBounds().width / 2, sprite.getLocalBounds().height / 2);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    // Main GUI loop
    while (mainWindow.isOpen())
    {
        // Process events
        sf::Event event;
        while (mainWindow.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);
            
            if (event.type == sf::Event::Closed)
                mainWindow.close();
        }
        ImGui::SFML::Update(mainWindow, deltaClock.restart());

        const auto currentFPSTime = std::chrono::steady_clock::now();
        const auto elapsedFPSTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentFPSTime - lastFPSDrawTime);

        sprite.setPosition(explorerView.getCenter());
        mainWindow.setView(explorerView);

        fpsText.setString(std::to_string(fps.getFPS()));
        fpsText.setCharacterSize(10);
        sf::Vector2f fpsPosition = mainWindow.mapPixelToCoords(sf::Vector2i(10, 10));
        fpsText.setPosition(fpsPosition);

        mainWindow.draw(fpsText);

        // Clear the main window
        mainWindow.clear(sf::Color{ 0, 0, 0, 255 });

        sf::RectangleShape explorerArea(sf::Vector2f(1280, 720));
        explorerArea.setFillColor(sf::Color{ 128, 128, 128, 255 });
        mainWindow.draw(explorerArea);

        if (particleShapes.size() > 0) {
            std::unique_lock lock(mtx);
            cv.wait(lock, [] { return readyToRender; });
            for (int i = 0; i < particleShapes.size(); i++) {
                mainWindow.draw(particleShapes[i]);
            }
            readyToCompute = true;
            readyToRender = false;
            cv.notify_all();
            lock.unlock();
        }

        mainWindow.draw(sprite);

        // Update the FPS counter
        fps.update();
        if (elapsedFPSTime >= timeInterval)
        {
            // Update last draw time
            lastFPSDrawTime = currentFPSTime;

        }
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowBgAlpha(0.25f); // Transparent background
        ImGui::SetNextWindowSize(ImVec2(75, 10));
        ImGui::Begin("FPS", nullptr, window_flags);
        ImGui::Text("FPS: %u", fps.getFPS());
        ImGui::End();

        ImGui::SFML::Render(mainWindow);
        
        // Display the contents of the main window
        mainWindow.display();
    }

    ImGui::SFML::Shutdown();
    closesocket(client_socket);
    WSACleanup();

    return 0;
}
