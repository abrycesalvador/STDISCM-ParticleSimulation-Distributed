#include <vector>
#include <iostream>
#include <SFML/Graphics.hpp>

class Wall {
public: 
	Wall(float x1, float y1, float x2, float y2) {
		this->x1 = x1;
		this->y1 = y1;
		this->x2 = x2;
		this->y2 = y2;
	}

private:
	float x1;
	float y1;
	float x2;
	float y2;

public:
	float getX1() {
		return x1;
	}

	float getY1() {
		return y1;
	}

	float getX2() {
		return x2;
	}

	float getY2() {
		return y2;
	}
};