#include <SFML/Graphics.hpp>
#include <iostream>
#include <math.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>


#include "Particle.cpp"
#include "FPS.cpp"

#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"

std::mutex mtx;
std::condition_variable cv;
bool readyToRender = false;
bool readyToCompute = true;
const int numThreads = std::thread::hardware_concurrency();
int currentParticle = 0;


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

int main()
{
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
    fpsText.setCharacterSize(30);
    fpsText.setFillColor(sf::Color::Green);
    fpsText.setPosition(1150, 680);
    fpsText.setStyle(sf::Text::Bold | sf::Text::Underlined);

	std::vector<Particle> particles;
	std::vector<sf::CircleShape> particleShapes;
    int particleCount = 0;

    
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


    sf::Clock deltaClock;

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
            
            for(int i = 0; i < numberParticles; i++){
				particles.push_back(Particle(i, startX + interval * i, startY + interval*i, angle, speed));
				particleShapes.push_back(sf::CircleShape(4, 10));
				particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
				//particleShapes.at(i).setFillColor(sf::Color::Red);
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
            if (numberParticles >1) interval = (angleEnd - angleStart) / (numberParticles);
                
            std::cout << interval;

			for (int i = 0; i < numberParticles; i++) {
				particles.push_back(Particle(i, startX2, startY2, angleStart+(interval*i), speed2));
				particleShapes.push_back(sf::CircleShape(4, 10));
				particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
				//particleShapes.at(i).setFillColor(sf::Color::Red);
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
				particles.push_back(Particle(i, startX3, startY3, angle3, speedStart + (interval * i)));
				particleShapes.push_back(sf::CircleShape(4, 10));
				particleShapes.at(i).setPosition(particles.at(i).getPosX(), particles.at(i).getPosY());
				//particleShapes.at(i).setFillColor(sf::Color::Red);
				particleCount++;
			}

            cv.notify_all();
        }

        if (ImGui::Button("Clear Balls"))
        {
            particleCount = 0;
            particles.clear();
            particleShapes.clear();
            //clear array of balls
        }

        ImGui::End();

        // Clear the main window
        mainWindow.clear(sf::Color::Black);    

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
            fpsText.setString(std::to_string(fps.getFPS()) + " FPS");
        }
        mainWindow.draw(fpsText);

        ImGui::SFML::Render(mainWindow);

        // Display the contents of the main window
        mainWindow.display();
    }

	for (auto& thread : threads) {
		thread.join();
	}

    ImGui::SFML::Shutdown();

    return 0;
}
