#include <SFML/Graphics.hpp>
#include <iostream>
#include <math.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>


#include "Particle.h"
#include "FPS.cpp"

#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"

std::mutex mtx;
std::condition_variable cv;
bool readyToRender = false;
bool readyToCompute = true;
const int numThreads = std::thread::hardware_concurrency();
int currentParticle = 0;
int mode = 1; // 0 - Dev; 1 - Explorer

sf::View explorerView(sf::FloatRect(645 - 9.5, 360 - 16.5, 33, 19));

bool activeClients[3] = { false, true, false};

sf::Sprite sprites[3];
sf::Texture textures[3];

float spritePositions[3][2]; 

std::vector<Particle> particles;
std::vector<sf::CircleShape> particleShapes;
int particleCount = 0;

std::atomic<bool> quitKeyPressed(false);
void moveExplorer(float moveX, float moveY);

void sendLocation(SOCKET client_socket, sf::View& explorer) {
    while (true) {

        sf::Vector2 position = explorer.getCenter();

        std::string sendString = "(1, " + std::to_string(position.x) + ", " + std::to_string(position.y) + ")";

        int bytes_sent = send(client_socket, sendString.c_str(), sendString.size(), 0);
        std::cout << "Sent: " << bytes_sent << std::endl;
        if (bytes_sent == SOCKET_ERROR) {
            std::cerr << "Error sending data to server" << std::endl;
            closesocket(client_socket);
            WSACleanup();
            break;
        }

        // send this thread every X seconds
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void receiveSpritePositions(SOCKET client_socket) {
    const int bufferSize = 1024;
    char buffer[bufferSize];
    int bytesReceived;

    while (true) {
        char buffer[sizeof(float) * 3 * 2];
        recv(client_socket, buffer, sizeof(buffer), 0);

        // Deserialize the data
        memcpy(spritePositions, buffer, sizeof(float) * 3 * 2);

        //print spritePositions
        for (int i = 0; i < 3; i++) {
            std::cout << "Sprite " << i << " X: " << spritePositions[i][0] << std::endl;
            std::cout << "Sprite " << i << " Y: " << spritePositions[i][1] << std::endl;

            if (spritePositions[i][0] == -1 && spritePositions[i][1] == -1) {
                activeClients[i] = false;
            }
            else {
                activeClients[i] = true;
            }
        }

        std::cout << std::endl;

    }
}

void keyboardInputListener() {
    while (!quitKeyPressed) {
        float moveX = 5, moveY = 2.5;   // Change values for how distance explorer moves.

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) {
            mode = (mode == 0) ? 1 : 0;
            std::cout << "Mode switched to: " << mode << std::endl;

            if (mode) {
                std::cout << "Last logged explorer X: " << explorerView.getCenter().x << std::endl;
                std::cout << "Last logged explorer Y: " << explorerView.getCenter().y << std::endl << std::endl;
            }//

            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Debounce time to avoid rapid mode switching

        }
        if (mode && (sf::Keyboard::isKeyPressed(sf::Keyboard::W) || sf::Keyboard::isKeyPressed(sf::Keyboard::Up))) {
            moveExplorer(0, -moveY);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Debounce time to avoid rapid movement
        }
        if (mode && (sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left))) {
            moveExplorer(-moveX, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (mode && (sf::Keyboard::isKeyPressed(sf::Keyboard::S) || sf::Keyboard::isKeyPressed(sf::Keyboard::Down))) {
            moveExplorer(0, moveY);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (mode && (sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right))) {
            moveExplorer(moveX, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

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
                cv.notify_one();
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

int main()
{
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

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(5001); // Port number of the server
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    if (connect(client_socket, reinterpret_cast<SOCKADDR*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Error connecting to server" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    // Create the main window
    sf::RenderWindow mainWindow(sf::VideoMode(1280, 720), "Particle Simulator Client 2");
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


    
    /*
    // SAMPLE PARTICLES
    for (int i = 0; i < numInitParticles; i++) {
		//particles.push_back(Particle(i, 100, 100, i, 5));
        particles.push_back(Particle(i, rand() % 1280, rand() % 720, rand() % 360, 5));
		particleShapes.push_back(sf::CircleShape(4, 10));
		particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
		particleShapes.at(i).setFillColor(sf::Color::Red);
		particleCount++;
	}*/

	std::vector<std::thread> threads;

	for (int i = 0; i < numThreads; ++i) {
		threads.emplace_back(updateParticles, std::ref(particles), std::ref(particleShapes));
	}

    std::thread keyboardThread(keyboardInputListener);

    std::thread sendLocationThread(sendLocation, client_socket, std::ref(explorerView));
    std::thread receiveSpritePositionsThread(receiveSpritePositions, client_socket);

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

        for (int i = 0; i < 3; ++i) {
            if (activeClients[i]) {
                sprites[i].setTexture(textures[i]);
                sprites[i].setTextureRect(sf::IntRect(0, 0, 3, 3));
                sprites[i].setOrigin(sprites[i].getLocalBounds().width / 2, sprites[i].getLocalBounds().height / 2);

                if (i == 1) {
                    sprites[i].setPosition(explorerView.getCenter());
                }
                else {
                    sprites[i].setPosition(spritePositions[i][0], spritePositions[i][1]);
                }
            }
        }

        mainWindow.setView(explorerView);

        fpsText.setString(std::to_string(fps.getFPS()));
        fpsText.setCharacterSize(10);

        sf::Vector2f fpsPosition = mainWindow.mapPixelToCoords(sf::Vector2i(10, 10));
        fpsText.setPosition(fpsPosition);


        mainWindow.draw(fpsText);
        //code for scaling - if using other images and not a color
        /*float desiredWidth = 1;
        float desiredHeight = 1;

        sf::FloatRect spriteBounds = sprite.getLocalBounds();
        float scaleX = desiredWidth / spriteBounds.width;
        float scaleY = desiredHeight / spriteBounds.height;

        sprite.setScale(scaleX, scaleY);*/


		

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
            currentParticle = 0;
            cv.notify_all();
            lock.unlock();
        }

        fps.update();

        if (elapsedFPSTime >= timeInterval)
        {
            // Update last draw time
            lastFPSDrawTime = currentFPSTime;

        }
        mainWindow.draw(fpsText);

        ImGui::SFML::Render(mainWindow);

        for (int i = 0; i < 3; ++i) {
            if (activeClients[i]) {
                mainWindow.draw(sprites[i]);
            }
        }
        
        // Display the contents of the main window
        mainWindow.display();
    }

	for (auto& thread : threads) {
		thread.join();
	}

    ImGui::SFML::Shutdown();

    closesocket(client_socket);
    WSACleanup();

    return 0;
}
