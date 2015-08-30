#pragma once
#include "../common/event.h"
#include "../common/machine.h"
#include "../common/string.h"
#include "../display/measurement.h"
#include "basic_types.h"
#include "constants.h"

namespace display {

class Color;
class Pen;

}  // namespace display

namespace ui {

class Feature;
class PlaceFeature;

}  // namespace ui

namespace engine {

class Combat;
class Detachment;
class Force;
class HexMap;
class ParcMap;
class PlaceDot;
class TerrainKeyItem;
class Theater;
class Unit;

// MapHeader is the on-disk storage format for the Hex Map.
struct MapHeader {
	int				cols;
	int				rows;
	float			latitude;		
	float			longitude;
	float			width;
	float			height;
	int				rotation;
	int				transportCnt;
	int				colors[58];
};

struct TerrainEdgeItem {
	float	moveCost[UC_MAXCARRIER];
};

class TransportData {
public:
	display::Pen*	pen;
	display::Pen*	penSmall;
	display::Pen*	penTiny;
	display::Color*	color;
	int				dx;
	int				dy;
	int				zoom;
	int				width;
	int				importance;
	int				tinyImportance;
	tons			fuel;
	int				railCap;				// 0 = not rail, 1 = single rail, 2 = double rail, etc.
	bool			negatesRivers;
	float			moveCost[UC_MAXCARRIER];
};

class Places {
public:
	Places(const string& filename);

	bool load(HexMap* map, int minimumImportance);

	void save(HexMap* map);

	PlaceDot* newPlaceDot();

	bool mapLocationToCoordinates(const string& loc, float* plat, float* plong);

	void insertAfter(PlaceDot* existing, PlaceDot* newDot);

	void removeNext(PlaceDot* existing);

	PlaceDot*	placeDots;
	bool		modified;
	
	const string& filename() const { return _filename; }

private:
	PlaceDot*	pdLast;
	string		_filename;
};

struct RoughModifier {
	float	move;
	float	density;
	float	defense;
	tons	fuel;
};

struct ModeTransition {
	int		duration;			// Duration of mode transition (in hours)
	tons	fuel;
};

class TerrainKey {
public:
	TerrainKey(const string& filename);

	int				keyCount;
	TerrainKeyItem*	table;
	/*
		carrierLength

		This table contains the road-length needed for each piece of equipment,
		according to its carrier classification.  This is a gross simplification.
		One might think that one should measure the length of each specific piece
		of equipment,
		but since realistic road march spacing was considerably larger than the
		vehicles themselves, more precise measurement seems kind of over-precise.
	 */
	float carrierLength[UC_MAXCARRIER];

	RoughModifier	roughModifier[4];
	ModeTransition modeTransition[UM_MAX][UM_MAX];



	bool load(HexMap* map);

	const string& filename() const { return _filename; }

private:
	string			_filename;
};

/*
	A HexMap object is the representation of the terrain map on which the game is played.  It is
	a hexagonal map.  Each hex is approximately ten kilometers across.

	This code is very specific to a particular layout and projection that was used to create the
	current game map.  If the projection is altered, the assumptions in this code will likely no
	longer apply.
 */
class HexMap {
public:
	HexMap(const string& filename, const string& placesFile, const string& terrainkeyFile, int rows, int cols);

	HexMap(const string& filename, ParcMap* parcMap, float kilometersPerHex, int rotation);

	~HexMap();

	bool load();

	void clean();

	bool equals(HexMap* map);

	xpoint findPlace(const string& p);

	void subset(xpoint pOrigin, xpoint pExtent);

	UnitCarriers calculateCarrier(Unit* u);

	EdgeValues edgeCrossing(xpoint p, HexDirection dir);
	/*
	 *	FUNCTION:	crossRiver
	 *
	 * This function returns true if the two input
	 * locations are separated by a major river hexside.
	 *
	 * This code assumes the hexes are, in fact, adjacent.
	 * The results are not valid if the hexes are not.
	 */
	bool crossRiver(xpoint hex1, xpoint hex2);
	/*
	 *	FUNCTION:	friendlyTaking
	 *
	 *	This function returns true if there is a friendly (to the force
	 *	argument) detachment already moving into the given hex.
	 */
	bool friendlyTaking(Detachment* dMover, xpoint hx);
	/*
	 *	FUNCTION: startingMeetingEngagement
	 *
	 *	This function returns true if there are units opposed to
	 *	the force parameter marching into the hex parameter.
	 */
	bool startingMeetingEngagement(xpoint hex, Force* force);
	/*
	traceLine: (parcMap: ParcMap, oldCell: int, newCell: int, x: int, y: int, edgec: EdgeValues)
	{
//		if (x != 1249 || y != 0)
//			return
//		traceFile.writeLine("tracing from [" + x + ":" + y + "] edge=" + edgec)
		parcMap.bitMap.setCell(x, y, newCell)
		findVertex(parcMap, x, y)
		traceBeyond(parcMap, oldCell, newCell, x, y, edgec)
		vertexChain = null
	}

	traceBeyond: (parcMap: ParcMap, oldCell: int, newCell: int, x: int, y: int, edgec: EdgeValues)
	{
		vstart := vertexChain
		for (;;){
			yn, xn: int

			for (i := 0; i < 8; i++){
				xn = x + adjacentx[i]
				yn = y + adjacenty[i]
				if (parcMap.bitMap.getCell(xn, yn) == oldCell)
					break
			}
			if (i >= 8)
				break
			parcMap.bitMap.setCell(xn, yn, newCell)
			for (; i < 8; i++){
				xx := x + adjacentx[i]
				yy := y + adjacenty[i]
				if (parcMap.bitMap.getCell(xx, yy) == oldCell)
					traceBeyond(parcMap, oldCell, newCell, x, y, edgec)
			}				
			x = xn
			y = yn
			findAdjacentVertex(parcMap, x, y)
		}
			// There must be one vertex in the chain, at least (so v.next test
			// works)

		for (v := vertexChain; v != vstart && v.next != null; v = v.next){

				// If the vertex numbers don't alternate, they can't be adjacent

			if (v.next.vertex == v.vertex)
				continue

//			traceFile.write("v = [" + v.p.x + ":" + v.p.y + "]." + v.vertex + " ")
			if (v.p.x == v.next.p.x &&
				v.p.y == v.next.p.y)
				setEdge(v.p.x, v.p.y - 1, 2, edgec)
			else if (v.vertex == 0){
				if (v.next.p.x != v.p.x - 1)
					continue
				if ((v.p.x & 1) == 1){
					if (v.next.p.y == v.p.y)
						setEdge(v.next.p.x, v.p.y, 0, edgec) 
					else if (v.next.p.y == v.p.y + 1)
						setEdge(v.next.p.x, v.p.y, 1, edgec)
				} else {
					if (v.next.p.y == v.p.y)
						setEdge(v.next.p.x, v.p.y - 1, 1, edgec)
					else if (v.next.p.y == v.p.y - 1)
						setEdge(v.next.p.x, v.p.y - 1, 0, edgec)
				}
			} else {
				if (v.next.p.x != v.p.x + 1)
					continue
				if ((v.p.x & 1) == 1){
					if (v.next.p.y == v.p.y)
						setEdge(v.p.x, v.p.y - 1, 1, edgec)
					else if (v.next.p.y == v.p.y + 1)
						setEdge(v.p.x, v.p.y, 0, edgec)
				} else {
					if (v.next.p.y == v.p.y - 1)
						setEdge(v.p.x, v.p.y - 1, 1, edgec)
					else if (v.next.p.y == v.p.y)
						setEdge(v.p.x, v.p.y, 0, edgec)
				}
			}
		}
		for (v = vertexChain; v != vstart; ){
			vnext := v.next
			v.next = freeVertex
			freeVertex = v
			v = vnext
		}
		vertexChain = vstart
	}

	findVertex: (parcMap: ParcMap, x: int, y: int)
	{
		p := parcMap.pixelToHex(x, y)
		y0 := parcMap.originy + p.y * parcMap.deltay
		y1 := y0 + parcMap.deltay / 2
		y2 := y0 + parcMap.deltay
		x0 := p.x * parcMap.deltax
		x1 := x0 + parcMap.deltax / 3
		x2 := x0 + parcMap.deltax
		x3 := x0 + (parcMap.deltax * 4) / 3
		v0dist := distance(x, y, x1, y0)
		v1dist := distance(x, y, x2, y0)
		v2dist := distance(x, y, x3, y1)
		v3dist := distance(x, y, x2, y2)
		v4dist := distance(x, y, x1, y2)
		v5dist := distance(x, y, x0, y1)
		minp := p
		minv := 0
		if (v0dist > v1dist){
			minv = 1
			v0dist = v1dist
		}
		if (v0dist > v2dist){
			if ((p.x & 1) == 1)
				minp.y++
			minp.x++
			minv = 0
			v0dist = v2dist
		}
		if (v0dist > v3dist){
			minp.x = p.x
			minp.y = xcoord(p.y + 1)
			minv = 1
			v0dist = v3dist
		}
		if (v0dist > v4dist){
			minp.x = p.x
			minp.y = xcoord(p.y + 1)
			minv = 0
			v0dist = v4dist
		}
		if (v0dist > v5dist){
			minp.x = xcoord(p.x - 1)
			minp.y = p.y
			if ((p.x & 1) == 1)
				minp.y++
			minv = 1
		}
		appendVertex(minp, minv)
	}

	findAdjacentVertex: (parcMap: ParcMap, x: int, y: int)
	{
		y0 := parcMap.originy + vertexChain.p.y * parcMap.deltay
		x0 := vertexChain.p.x * parcMap.deltax + parcMap.deltax / 3
		minp := vertexChain.p
		minv := vertexChain.vertex
		if (vertexChain.vertex == 0){
			y1 := y0
			x1 := x0 + 2 * parcMap.deltax / 3
			y2 := y0 - parcMap.deltay / 2
			x2 := x0 - parcMap.deltax / 3
			y3 := y0 + parcMap.deltay / 2
			x3 := x2
			v0dist := distance(x, y, x0, y0)
			v1dist := distance(x, y, x1, y1)
			v2dist := distance(x, y, x2, y2)
			v3dist := distance(x, y, x3, y3)
			if (v0dist > v1dist){
				minv = 1
				v0dist = v1dist
			}
			if (v0dist > v2dist){
				if ((minp.x & 1) == 0)
					minp.y--
				minp.x--
				minv = 1
				v0dist = v2dist
			}
			if (v0dist > v3dist){
				if ((vertexChain.p.x & 1) == 1)
					minp.y = xcoord(vertexChain.p.y + 1)
				minp.x = xcoord(vertexChain.p.x - 1)
				minv = 1
			}
		} else {
			y1 := y0
			x1 := x0 - 2 * parcMap.deltax / 3
			y2 := y0 - parcMap.deltay / 2
			x2 := x0 + parcMap.deltax / 3
			y3 := y0 + parcMap.deltay / 2
			x3 := x2
			v0dist := distance(x, y, x0, y0)
			v1dist := distance(x, y, x1, y1)
			v2dist := distance(x, y, x2, y2)
			v3dist := distance(x, y, x3, y3)
			if (v0dist > v1dist){
				minv = 0
				v0dist = v1dist
			}
			if (v0dist > v2dist){
				if ((minp.x & 1) == 0)
					minp.y--
				minp.x++
				minv = 0
				v0dist = v2dist
			}
			if (v0dist > v3dist){
				if ((vertexChain.p.x & 1) == 1)
					minp.y = xcoord(vertexChain.p.y + 1)
				minp.x = xcoord(vertexChain.p.x + 1)
				minv = 0
			}
		}
		if (minv != vertexChain.vertex){
//			traceFile.write("[" + x + ":" + y + "] ")
			appendVertex(minp, minv)
		}
	}

	appendVertex: (p: xpoint, vv: int)
	{

			// If it's the same as the last vertex, skip it

		if (vertexChain != null &&
			vertexChain.p.x == p.x &&
			vertexChain.p.y == p.y &&
			vertexChain.vertex == vv)
			return
//		traceFile.writeLine("v=[" + p.x + ":" + p.y + "]." + vv)
		v: Vertex
		if (freeVertex != null){
			v = freeVertex
			freeVertex = v.next
			v.p = p
			v.vertex = vv
		} else
			v = new Vertex(p, vv)
		v.next = vertexChain
		vertexChain = v
	}
 */
    bool save();

	void mergeElev(const string& filename, int lowThreshold, int middleThreshold, int highThreshold);

	void mergeRough(const string& filename, int lowThreshold, int middleThreshold, int highThreshold);

	float hexToLatitude(xpoint hx);

	float hexToLongitude(float latitude, xpoint hx);

	xpoint latLongToHex(float lat0, float long0);

	display::point latLongToPixel(float lat0, float long0, float zoom);

	float hexScale();

	spherePoint hexToSphere(xpoint hx);

	xpoint sphereToHex(const spherePoint& p);

	display::point sphereToPixel(const spherePoint& p, float zoom);

	spherePoint pixelToSphere(display::point p, float zoom);

    void setCell(xpoint hx, int index);

	void setFeature(xpoint p, ui::Feature* f);

	void addFeature(xpoint x, ui::Feature* f);

	void hideFeature(xpoint p, ui::Feature* f);

	int getVictoryPoints(xpoint hex);

	void setTransportEdge(xpoint hx, int e, TransFeatures f);

	ui::PlaceFeature* getPlace(xpoint hx);

	float getFortification(xpoint hx);

	void setFortification(xpoint hx, float level);

	TransFeatures getTransportEdge(xpoint hx, int e);

	void clearTransportEdge(xpoint hx, int e, TransFeatures f);

	void createDefaultBridges(const Theater* theater);

	void setEdge(xpoint hx, int e, EdgeValues ve);

	int getCell(xpoint hx);

	ui::Feature* getFeature(xpoint hx);

    EdgeValues getEdge(xpoint hx, int e);

	float getDefense(xpoint hx);

	Detachment* getDetachments(xpoint hx);

	Combat* combat(xpoint hx) const;

	void set_combat(xpoint hx, Combat* c);
	/*
	 *	FUNCTION: attackingFrom
	 *
	 *	This function returns true if there is at least one
	 *	unit attacking from location loc1 to location loc2.
	 *
	 *	Units cannot start moving from a hex into a hex from
	 *	which they are being attacked.  They must wait until the
	 *	combat is finished.
	 */
	bool attackingFrom(xpoint loc1, xpoint loc2);
	/*
	 *	FUNCTION:	enemyZoc
	 *
	 *	This function returns true if the hex in question is in an enemy
	 *	zone of control.  Where the enemy is any other force than the one
	 *	given.
	 */
	bool enemyZoc(Force* force, xpoint hx);
	/*
	 *	FUNCTION:	enemyContact
	 *
	 *	This function returns true if the hex in question is in an enemy
	 *	zone of control.  Where the enemy is any other force than the one
	 *	given.
	 */
	bool enemyContact(Force* force, xpoint hx);
	/*
	 *	FUNCTION: isFriendly
	 *
	 *	This function returns true if the hex is friendly to the given force
	 */
	bool isFriendly(Force* force, xpoint hx);

	void remove(Detachment* d);

	void place(Detachment* d);

	void placeOnTop(Detachment* d);			// Testing support API

	void setOccupier(xpoint hx, int index);

	int getOccupier(xpoint hx);

	float getDensity(xpoint hx);

	float getRoughDefense(xpoint hx);

	float getRoughDensity(xpoint hx);

	bool hasRails(xpoint hx);

	int getRows();

	int getColumns();

	bool valid(const xpoint& p) const {
		return p.x >= _subsetOrigin.x &&
			   p.x < _subsetOpposite.x &&
			   p.y >= _subsetOrigin.y &&
			   p.y < _subsetOpposite.y;
	}

	MapHeader		header;
	string			filename;
	TerrainEdgeItem terrainEdge[4];
	TransportData	transportData[16];
	Places			places;
	TerrainKey		terrainKey;

	Event1<xpoint>	changed;

	xpoint subsetOrigin() const { return _subsetOrigin; }
	xpoint subsetOpposite() const { return _subsetOpposite; }

private:
	class Hex {
	public:
		unsigned		data;
		byte			occupier;				// Combatant who formally occupies this hex.
		ui::Feature*	features;
		Detachment*		detachments;
		Combat*			combat;
	};

	Hex& hex(xpoint hx) const { return _hexes[hx.y * _rowSize + hx.x]; }

	int				_rowSize;
	int				_allocatedRows;

	// Both of the following are arrays rowSize by allocatedRows big.
	Hex*			_hexes;
	xpoint			_subsetOrigin;
	xpoint			_subsetOpposite;
};

class PlaceDot {
public:
	PlaceDot();

	PlaceDot*			next;
	int					importance;
	float				latitude;
	float				longitude;
	string				name;
	string				country;
	string				native;
	string				dsg;
	bool				drawPeak;
	bool				physicalFeature;
	display::Color*		textColor;

	string uniq();
};

class TerrainKeyItem {
public:
	int		color;				// rgb mask
	float	moveCost[UC_MAXCARRIER];
	float	density;
	float	defense;
	tons	fuel;
};

HexMap* loadHexMap(const string& filename, const string& placesFile, const string& terrainKeyFile);

void normalize(xpoint* hx, int* edge);

extern bool loadPlaceDots;

}  // namespace engine
