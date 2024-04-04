#include <vector>
#include <iostream>
#include <SFML/Graphics.hpp>
#include "cmath"
#include <sstream>

#define PI 3.14159265358979323846

class Particle {
public:
	Particle(int id, float posX, float posY, float  angleDeg, float speed) {
		// convert to radians
		this->particleVector = { 0, 0 };
		this->id = id;
		this->angle = angleDeg * PI / 180;
		this->speed = speed;
		this->posX = posX;
		this->posY = posY;
		//std::cout << "ID: " << this->id << " | speed: " << this->speed << std::endl;

		setParticleVector();
	}

	Particle(int id, float posX, float posY) {
		this->particleVector = { 0, 0 };
		this->id = id;
		this->posX = posX;
		this->posY = posY;
		this->angle = 0;
		this->speed = 0;
	}

private:
	std::vector<float> particleVector;
	float angle;
	float speed;
	float posX;
	float posY;
	int id;

public:

	void setParticleVector() {
		particleVector[0] = this->speed * cos(this->angle);
		particleVector[1] = this->speed * sin(this->angle);
	}

	void updateParticlePosition() {
		posX += particleVector[0];
		posY += particleVector[1];
	}
		
	void checkCollision() {
		if (posX > 1280 || posX < 0)
			particleVector.at(0) = -particleVector.at(0);
		if (posY > 720 || posY < 0)
			particleVector.at(1) = -particleVector.at(1);
	}

	int getId() {
		return id;
	}

	float getPosX() {
		return posX;
	}

	float getPosY() {
		return posY;
	}

	std::string serialize() {
		std::stringstream ss;
		ss << "(" << id << "," << posX << "," << posY << ")";
		return ss.str();
	}

	static Particle deserialize(const std::string& str) {
		std::stringstream ss(str);
		int id;
		float posX, posY, angle, speed;
		char c;
		ss >> c >> id >> c >> posX >> c >> posY >> c;


		return Particle(id, posX, posY);
	}
};