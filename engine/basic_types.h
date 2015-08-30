#pragma once

namespace engine {

typedef float tons;
typedef unsigned minutes;

typedef short	xcoord;

struct xpoint {
	xcoord	x, y;

	xpoint() {
		this->x = 0;
		this->y = 0;
	}

	xpoint(int x, int y) {
		this->x = x;
		this->y = y;
	}

	bool operator ==(const xpoint& hx) const {
		return x == hx.x && y == hx.y;
	}

	bool operator !=(const xpoint& hx) const {
		return x != hx.x || y != hx.y;
	}
};

struct spherePoint {
	float	latitude;
	float	longitude;
};

typedef int HexDirection;

}  // namespace engine
