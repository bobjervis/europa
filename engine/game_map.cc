#include "../common/platform.h"
#include "game_map.h"

#include <math.h>
#include "../common/file_system.h"
#include "../common/machine.h"
#include "../ui/map_ui.h"
#include "../ui/ui.h"
#include "bitmap.h"
#include "combat.h"
#include "detachment.h"
#include "engine.h"
#include "force.h"
#include "game.h"
#include "global.h"
#include "path.h"
#include "theater.h"
#include "unit.h"

namespace engine {

const short MAGIC_NUMBER = short(('M' << 8) + 'X');

struct TransportDescriptor {
	int				index;
	unsigned char	edgeSet[3];
//	filler:			byte
};

static float intercepts[2];

//EQUATOR:	public const int = 862//849			// y coord of equator
//POLE:		public const int = 1008//993			// diff between pole and equator
//GREENWICH:	public const int = 203//200
//DATELINE:	public const int = 2328

	// note in elev bitmaps, the y coords number from 0 starting at bottom of map

//ELEV_EQUATOR:	public const int = 1350		// y coord of equator
//ELEV_POLE:		public const int = 4050//1350 * 3 // y coord of north pole
//ELEV_GREENWICH:	public const int = 1350		// x coord of greenwich meridian
//ELEV_DATELINE:	public const int = 6750//1350 * 5	// x coord of intl dateline
const float ELEV_LATITUDE = -45;
const float ELEV_LONGITUDE = -45;

HexMap::HexMap(const string &filename, const string& placesFile, const string& terrainKeyFile, int rows, int cols) : places(placesFile), terrainKey(terrainKeyFile) {
	this->filename = filename;
	this->header.cols = cols;
	this->header.rows = rows;
	this->header.height = 0;
	this->header.latitude = 0;
	this->header.longitude = 0;
	this->header.rotation = 0;
	this->header.transportCnt = 0;
	this->header.width = 0;
	_rowSize = cols;
	_allocatedRows = rows;
	memset(transportData, 0, sizeof transportData);
	int data_length = header.cols * header.rows;
	if (data_length) {
		_hexes = new Hex[data_length];
		memset(_hexes, 0, data_length * sizeof(Hex));
	} else
		_hexes = null;
}
/*
	new: (filename: string, parcMap: ParcMap, kilometersPerHex: float, rotation: int)
	{
		this.filename = filename
		p := parcMap.hexScale(kilometersPerHex, rotation)
		rows = p.y
		cols = p.x
		rowSize = cols
		allocatedRows = rows
		latitude = parcMap.latitude
		longitude = parcMap.longitude
		width = parcMap.width
		height = parcMap.height
		this.rotation = rotation

//		backDeltaX = parcMap.deltax * sqrt3 / 1.5f
//		backDeltaY = parcMap.deltay

		data = unmanaged new [rows * cols] unsigned[16]
		features = unmanaged new [rows * cols] pointer ui.Feature

		hx: xpoint
		for (hx.y = 0; hx.y < rows; hx.y++){
			for (hx.x = 0; hx.x < cols; hx.x++){
				setCell(hx, TRANSPARENT)
			}
		}
//		traceFile = fileSystem.createTextFile("trace.txt")

		for (y := 0; y < parcMap.bitMap.rows; y++){
			for (x := 0; x < parcMap.bitMap.cols; x++){
				c := parcMap.bitMap.getCell(x, y)
				if (c == PARC_BORDER)
					traceLine(parcMap, PARC_BORDER, PARC_XBORDER, x, y, EDGE_BORDER)
			}
		}

 		for (y = 0; y < parcMap.bitMap.rows; y++){
			for (x := 0; x < parcMap.bitMap.cols; x++){
				c := parcMap.bitMap.getCell(x, y)
				if (c == PARC_RIVER){
					traceLine(parcMap, PARC_RIVER, PARC_XRIVER, x, y, EDGE_RIVER)
				}
			}
		}

		for (y = 0; y < parcMap.bitMap.rows; y++){
			for (x := 0; x < parcMap.bitMap.cols; x++){
				c := parcMap.bitMap.getCell(x, y)
				if (c == PARC_COAST)
					traceLine(parcMap, PARC_COAST, PARC_XCOAST, x, y, EDGE_COAST)
			}
		}

//		traceFile.close()

		for (hx.y = 0; hx.y < rows; hx.y++){
			for (hx.x = 0; hx.x < cols; hx.x++){
				for (e := 0; e < 3; e++){
					edge := getEdge(hx.x, hx.y, e)
					if (edge == EDGE_RIVER || edge == EDGE_BORDER){
						e0, e1, e2, e3: EdgeValues
						switch (e){
						case	0:
							e0 = getEdge(hx.x, hx.y - 1, 1)
							e1 = getEdge(hx.x, hx.y - 1, 2)
							e2 = getEdge(hx.x, hx.y, 1)
							if ((hx.x & 1) == 1){
								e3 = getEdge(hx.x + 1, hx.y, 2)
							} else {
								e3 = getEdge(hx.x + 1, hx.y - 1, 2)
							}
							break

						case	1:
							e0 = getEdge(hx.x, hx.y, 0)
							e2 = getEdge(hx.x, hx.y, 2)
							e3 = getEdge(hx.x, hx.y + 1, 0)
							if ((hx.x & 1) == 1){
								e1 = getEdge(hx.x + 1, hx.y, 2)
							} else {
								e1 = getEdge(hx.x + 1, hx.y - 1, 2)
							}
							break

						case	2:
							e0 = getEdge(hx.x, hx.y, 1)
							e1 = getEdge(hx.x, hx.y + 1, 0)
							if ((hx.x & 1) == 1){
								e2 = getEdge(hx.x - 1, hx.y + 1, 0)
								e3 = getEdge(hx.x - 1, hx.y + 1, 1)
							} else {
								e2 = getEdge(hx.x - 1, hx.y, 0)
								e3 = getEdge(hx.x - 1, hx.y, 1)
							}
						}
						if ((e0 == edge && e1 == edge && e2 != edge && e3 != edge) ||
							(e0 != edge && e1 != edge && e2 == edge && e3 == edge))
							setEdge(hx.x, hx.y, e, EDGE_PLAIN)
					}
				}
				x0 := int(hx.x * parcMap.deltax)
				yAdjust := 0.0
				if ((hx.x & 1) != 0)
					yAdjust = 0.5
				y0 := parcMap.originy + int((hx.y + yAdjust) * parcMap.deltay)
				x1 := int((hx.x + 4.0 / 3) * parcMap.deltax)
				y1 := parcMap.originy + int((hx.y + 1 + yAdjust) * parcMap.deltay)
//				coast := 0
				ocean := 0
//				river := 0
				land := 0
				uncertain := 0
				if (y0 <= y1)
					y1 = y0 - 1
				if (x0 >= x1)
					x1 = x0 + 1
				for (by := y0; by > y1; by--){
					for (bx := x0; bx < x1; bx++){
						switch (parcMap.bitMap.getCell(bx, by)){
						case	PARC_WHITE:
						case	PARC_GRID:
						case	PARC_BORDER:
						case	PARC_XBORDER:
							uncertain++
							break

						case	PARC_COAST:
						case	PARC_XCOAST:
						case	PARC_BLUE:
							ocean++
							break

						case	PARC_WADI:
						case	PARC_CANAL:
							break

						case	PARC_XRIVER:
						case	PARC_RIVER:
						case	PARC_DEPRESSION:
						case	PARC_GREY:
						case	PARC_RED:
							land++
							break
						}						
					}
				}
				pixelCount := (y0 - y1) * (x1 - x0)
				if (land > 0){
					if (land + uncertain == pixelCount)
						setCell(hx, TAIGA1)
				}
				if (ocean > 0){
					if (ocean == pixelCount)
						setCell(hx, DEEP_WATER)
				}
			}
		}
		changed: boolean
		do {
			changed = false
			for (hx.y = 0; hx.y < rows; hx.y++){
				for (hx.x = 0; hx.x < cols; hx.x++){
					c := getCell(hx.x, hx.y)
					if (c == TRANSPARENT){
						ac: [6] int
						ae: [6] EdgeValues

						ac[0] = getCell(hx.x, hx.y - 1)
						ae[0] = getEdge(hx.x, hx.y - 1, 2)
						ac[1] = getCell(hx.x, hx.y + 1)
						ae[1] = getEdge(hx.x, hx.y, 2)
						ac[2] = getCell(hx.x + 1, hx.y)
						ac[3] = getCell(hx.x - 1, hx.y)
						if ((hx.x & 1) == 1){
							ae[2] = getEdge(hx.x, hx.y, 0)
							ae[3] = getEdge(hx.x - 1, hx.y, 1)
							ac[4] = getCell(hx.x + 1, hx.y + 1)
							ae[4] = getEdge(hx.x, hx.y, 1)
							ac[5] = getCell(hx.x - 1, hx.y + 1)
							ae[5] = getEdge(hx.x - 1, hx.y + 1, 0)
						} else {
							ae[2] = getEdge(hx.x, hx.y, 1)
							ae[3] = getEdge(hx.x - 1, hx.y, 0)
							ac[4] = getCell(hx.x + 1, hx.y - 1)
							ae[4] = getEdge(hx.x, hx.y, 0)
							ac[5] = getCell(hx.x - 1, hx.y - 1)
							ae[5] = getEdge(hx.x - 1, hx.y - 1, 1)
						}
						land := 0
						ocean := 0
						for (i := 0; i < 6; i++){
							if (ae[i] == EDGE_COAST)
								ac[i] = TRANSPARENT
							if (ac[i] == TAIGA1)
								land++
							else if (ac[i] == DEEP_WATER)
								ocean++
						}
						if (land > 0 && ocean == 0){
							changed = true
							setCell(hx, TAIGA1)
						} else if (land == 0 && ocean > 0){
							changed = true
							setCell(hx, DEEP_WATER)
						}
					}
				}
			}
		} while (changed)
	}
 */

HexMap::~HexMap() {
	delete [] _hexes;
}

bool HexMap::load() {
	FILE* fp = fileSystem::openBinaryFile(filename);
	if (fp == null)
		return false;
	short magic;

	if (fread(&magic, sizeof magic, 1, fp) != 1) {
		fclose(fp);
		return false;
	}
	if (magic != MAGIC_NUMBER) {
		fclose(fp);
		return false;
	}
	if (fread(&header, sizeof header, 1, fp) != 1) {
		fclose(fp);
		return false;
	}
	_rowSize = header.cols;
	_allocatedRows = header.rows;
	_subsetOrigin.x = 0;
	_subsetOrigin.y = 0;
	_subsetOpposite.x = header.cols;
	_subsetOpposite.y = header.rows;

	int data_length = header.cols * header.rows;
	if (_hexes == null) {
		_hexes = new Hex[data_length];
		memset(_hexes, 0, data_length * sizeof(Hex));
	}

	for (int i = 0; i < data_length; i++) {
		unsigned short x;

		if (fread(&x, sizeof x, 1, fp) != 1) {
			fclose(fp);
			return false;
		}
		_hexes[i].data = x;
	}

	TransportDescriptor* td = new TransportDescriptor[header.transportCnt];
	int actualLen = fread(td, sizeof (TransportDescriptor), header.transportCnt, fp);
	if (actualLen != header.transportCnt) {
		fclose(fp);
		return false;
	}

	for (int t = 0; t < header.transportCnt; t++){
		xpoint hx;

		int i = td[t].index;
		hx.x = (xcoord)(i % header.cols);
		hx.y = (xcoord)(i / header.cols);
		for (int j = 0; j < 3; j++)
			if (td[t].edgeSet[j] != 0)
				setTransportEdge(hx, j, TransFeatures(td[t].edgeSet[j]));
	}

		// The game will put bridges back in for the roads that need them.

	xpoint hx;
	for (hx.x = 0; hx.x < header.cols; hx.x++)
		for (hx.y = 0; hx.y < header.rows; hx.y++){
			for (int j = 0; j < 3; j++)
				clearTransportEdge(hx, j, TF_BRIDGE);
		}
	fclose(fp);
	if (!places.load(this, 0))
		return false;
	global::kmPerHex = hexScale();
	return terrainKey.load(this);
}

void HexMap::clean() {
	xpoint hx;
	for (hx.x = 0; hx.x < header.cols; hx.x++)
		for (hx.y = 0; hx.y < header.rows; hx.y++) {
			for (int j = 0; j < 3; j++)
				clearTransportEdge(hx, j, TransFeatures(TF_BLOWN_BRIDGE|TF_RAIL_CLOGGED|TF_CLOGGED));
			Hex& h = hex(hx);
			if (h.combat != null) {
				delete h.combat;
				h.combat = null;
			}
		}
	for (hx.x = 0; hx.x < header.cols; hx.x++)
		for (hx.y = 0; hx.y < header.rows; hx.y++) {
			for (;;) {
				Detachment* d = getDetachments(hx);
				if (d == null)
					break;
				d->unit->hide();
				delete d;
			}
		}
}

bool HexMap::equals(HexMap* map) {
	if (_subsetOrigin != map->_subsetOrigin ||
		_subsetOpposite != map->_subsetOpposite ||
		header.cols != map->header.cols ||
		header.rows != map->header.rows)
		return false;
	xpoint hx;
	for (hx.x = 0; hx.x < header.cols; hx.x++)
		for (hx.y = 0; hx.y < header.rows; hx.y++) {
			if (getFortification(hx) != map->getFortification(hx))
				return false;
			if (getOccupier(hx) != map->getOccupier(hx))
				return false;
		}
	return true;
}

xpoint HexMap::findPlace(const string& p) {
	xpoint hx;

	for (hx.x = _subsetOrigin.x; hx.x < _subsetOpposite.x; hx.x++)
		for (hx.y = _subsetOrigin.y; hx.y < _subsetOpposite.y; hx.y++){
			ui::Feature* sf = getFeature(hx);
			if (sf == null)
				continue;
			for (ui::Feature* f = sf; ; f = f->next()){
				if (f->featureClass() == ui::FC_PLACE) {
					ui::PlaceFeature* pf = (ui::PlaceFeature*)f;
					if (pf->name == p)
						return hx;
				}
				if (f->next() == sf)
					break;
			}
		}
	hx.x = -1;
	hx.y = -1;
	return hx;
}

void HexMap::subset(xpoint pOrigin, xpoint pExtent) {
	if (pExtent.x == 0 &&
		pExtent.y == 0) {
		_subsetOrigin.x = 0;
		_subsetOrigin.y = 0;
		_subsetOpposite.x = header.cols;
		_subsetOpposite.y = header.rows;
		return;
	}
	_subsetOrigin = pOrigin;
	_subsetOpposite.x = pOrigin.x + pExtent.x;
	if (_subsetOpposite.x > header.cols)
		_subsetOpposite.x = header.cols;
	_subsetOpposite.y = pOrigin.y + pExtent.y;
	if (_subsetOpposite.x > header.rows)
		_subsetOpposite.x = header.rows;
}

UnitCarriers HexMap::calculateCarrier(Unit* u) {
	MoveInfo mi;

	memset(&mi, 0, sizeof mi);
	u->calculate(&mi);
	const TerrainKeyItem& tki = terrainKey.table[CLEARED];
	for (int i = UC_MINCARRIER; i < UC_MAXCARRIER; i++)
		mi.rawCost[i] = int(1000 / tki.moveCost[i]);
	deriveMoveCost(u, &mi);
	return mi.carrier;
}

EdgeValues HexMap::edgeCrossing(xpoint p, HexDirection dir) {
	static int edgeXp[6] = { 0, 0, 0, 0, -1, -1 };
	static int edgeYp[2][6] = {
						{ -1, 0, 0, 0, 0, -1 },
						{ -1, 0, 0, 0, 1, 0 }
	};
	static int edgeEp[6] = { 2, 0, 1, 2, 0, 1 };

	xpoint hx;
	hx.x = p.x + edgeXp[dir];
	hx.y = p.y + edgeYp[p.x & 1][dir];
	int e = edgeEp[dir];
	return getEdge(hx, e);
}
/*
 *	FUNCTION:	crossRiver
 *
 * This function returns true if the two input
 * locations are separated by a major river hexside.
 *
 * This code assumes the hexes are, in fact, adjacent.
 * The results are not valid if the hexes are not.
 */
bool HexMap::crossRiver(xpoint hex1, xpoint hex2) {
	HexDirection hd = directionTo(hex1, hex2);
	EdgeValues e = edgeCrossing(hex1, hd);
	return e == EDGE_COAST;
}
/*
 *	FUNCTION:	friendlyTaking
 *
 *	This function returns true if there is a friendly (to the force
 *	argument) detachment already moving into the given hex.
 */
bool HexMap::friendlyTaking(Detachment* dMover, xpoint hx) {
	Force* force = dMover->unit->combatant()->force;
	for (HexDirection i = 0; i < 6; i++) {
		for (Detachment* d = getDetachments(neighbor(hx, i)); d != null; d = d->next) {
			if (d->unit->combatant()->force != force)
				break;
			if (d != dMover && 
				d->action == DA_MARCHING &&
				d->destination.x == hx.x &&
				d->destination.y == hx.y)
				return true;
		}
	}
	return false;
}
/*
 *	FUNCTION: startingMeetingEngagement
 *
 *	This function returns true if there are units opposed to
 *	the unit parameter marching into the hex parameter.
 */
bool HexMap::startingMeetingEngagement(xpoint hex, Force* force) {
	for (HexDirection i = 0; i < 6; i++){
		xpoint hx = neighbor(hex, i);
		Detachment* d = getDetachments(hx);
		if (d == null)
			continue;
		if (!d->unit->opposes(force))
			continue;
		do {
			if (d->entering(hex))
				return true;
			d = d->next;
		} while (d != null);
	}
	return false;
}

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
bool HexMap::save() {
	if (!fileSystem::createBackupFile(filename)) {
		warningMessage("Couldn't create backup file for: " + filename);
		return false;
	}
	FILE* fp = fileSystem::createBinaryFile(filename);
	if (fp == null) {
		warningMessage("Couldn't create file: " + filename);
		return false;
	}
	short magic = MAGIC_NUMBER;
	if (fwrite(&magic, sizeof magic, 1, fp) != 1) {
		warningMessage("Write error on file: " + filename);
		fclose(fp);
		return false;
	}
	header.transportCnt = 0;
	for (int i = 0; i < _allocatedRows * _rowSize; i++) {
		ui::Feature* f = _hexes[i].features;
		if (f == null)
			continue;
		ui::Feature* fx = f;
		do {
			if (fx->featureClass() == ui::FC_TRANSPORT)
				header.transportCnt++;
			fx = fx->next();
		} while (fx != f);
	}
	if (fwrite(&header, sizeof header, 1, fp) != 1) {
		warningMessage("Write error on file: " + filename);
		fclose(fp);
		return false;
	}
	for (int i = 0; i < _allocatedRows * _rowSize; i++) {
		if (fwrite(&_hexes[i].data, sizeof (unsigned short), 1, fp) != 1) {
			warningMessage("Write error on file: " + filename);
			fclose(fp);
			return false;
		}
	}
	TransportDescriptor* td = new TransportDescriptor[header.transportCnt];
	int t = 0;
	for (int i = 0; i < _allocatedRows * _rowSize; i++) {
		ui::Feature* f = _hexes[i].features;
		if (f == null)
			continue;
		ui::Feature* fx = f;
		do {
			if (fx->featureClass() == ui::FC_TRANSPORT) {
				ui::TransportFeature* tf = (ui::TransportFeature*)fx;
				td[t].index = i;
				td[t].edgeSet[0] = byte(tf->edge(0));
				td[t].edgeSet[1] = byte(tf->edge(1));
				td[t].edgeSet[2] = byte(tf->edge(2));
				t++;
			}
			fx = fx->next();
		} while (fx != f);
	}
	if (fwrite(td, sizeof TransportDescriptor, t, fp) != t) {
		warningMessage("Write error on file: " + filename);
		fclose(fp);
		return false;
	}
	fclose(fp);
	return true;
}

void HexMap::mergeElev(const string& filename, int lowThreshold, int middleThreshold, int highThreshold) {
	BitMap* b = loadBitMap(filename);
	xpoint hx;
	for (hx.y = 0; hx.y < header.rows; hx.y++) {
		for (hx.x = 0; hx.x < header.cols; hx.x++) {
			int v = getCell(hx);
			int t = v & 0xf;

				// only alter 'normal' terrains

			if (t < 6)
				continue;
			float lat = hexToLatitude(hx);
			xpoint hx1 = hx;
			hx1.x++;
			hx1.y++;
			float lat2 = hexToLatitude(hx1);
			float long1 = hexToLongitude(lat, hx);
			float long2 = hexToLongitude(lat2, hx1);
			if (lat > lat2) {
				float x = lat;
				lat = lat2;
				lat2 = x;
			}
			if (long1 > long2) {
				float x = long1;
				long1 = long2;
				long2 = x;
			}
			v &= 0x3f;		// remove the elevation
			int elevx0 = int((long1 - ELEV_LONGITUDE) * 30);
			int elevx1 = int((long2 - ELEV_LONGITUDE) * 30);
			int elevy0 = int((lat - ELEV_LATITUDE) * 30);
			int elevy1 = int((lat2 - ELEV_LATITUDE) * 30);
			int elevation = 0;
			int pixels = 0;
			if (elevx0 == elevx1)
				elevx1 = elevx0 + 1;
			if (elevy0 == elevy1)
				elevy1 = elevy0 + 1;
			int patchSize = (elevx1 - elevx0) * (elevy1 - elevy0);
			int highPixels = 0;
			for (int ey = elevy0; ey < elevy1; ey++) {
				for (int ex = elevx0; ex < elevx1; ex++) {
					int c = b->getCell(ex, ey);
					if (c == 254) {
						highPixels++;
					} else if (c >= 1 && c <= 252) {
						pixels++;
						elevation += c;
					}
				}
			}
			if (pixels == 0) {
				if (highPixels == patchSize)
					setCell(hx, 4);	// alpine
				else
					setCell(hx, v);	// corresponds to water - leave alone
				continue;
			}
			float r = float(elevation) / pixels;
			int e = 0;
			if (r >= highThreshold)
				e = 3;
			else if (r >= middleThreshold)
				e = 2;
			else if (r >= lowThreshold)
				e = 1;
			if (e + ((v >> 4) & 3) > 3)
				setCell(hx, MOUNTAIN);
			else
				setCell(hx, v + (e << 6));
		}
	}
}

void HexMap::mergeRough(const string& filename, int lowThreshold, int middleThreshold, int highThreshold) {
	BitMap* b = loadBitMap(filename);
	xpoint hx;
	for (hx.y = 0; hx.y < header.rows; hx.y++) {
		for (hx.x = 0; hx.x < header.cols; hx.x++) {
			int v = getCell(hx);
			int t = v & 0xf;

				// only alter 'normal' terrains

//				if (t < 6)
//					continue
			float lat = hexToLatitude(hx);
			xpoint hx1 = hx;
			hx1.x++;
			hx1.y++;
			float lat2 = hexToLatitude(hx1);
			float long1 = hexToLongitude(lat, hx);
			float long2 = hexToLongitude(lat2, hx1);
			if (lat > lat2) {
				float x = lat;
				lat = lat2;
				lat2 = x;
			}
			if (long1 > long2) {
				float x = long1;
				long1 = long2;
				long2 = x;
			}
			v &= 0x3f;		// remove the elevation
			int elevx0 = int((long1 - ELEV_LONGITUDE) * 30);
			int elevx1 = int((long2 - ELEV_LONGITUDE) * 30);
			int elevy0 = int((lat - ELEV_LATITUDE) * 30);
			int elevy1 = int((lat2 - ELEV_LATITUDE) * 30);
			int roughness = 0;
			int pixels = 0;
			bool isTransparent = v == TRANSPARENT_HEX;
			v &= ~0x30;					// reduce the cell value to the 'flat' value
			if (elevx0 == elevx1)
				elevx1 = elevx0 + 1;
			if (elevy0 == elevy1)
				elevy1 = elevy0 + 1;
			int patchSize = (elevx1 - elevx0) * (elevy1 - elevy0);
			int highPixels = 0;
			for (int ey = elevy0; ey < elevy1; ey++) {
				for (int ex = elevx0; ex < elevx1; ex++) {
					int c = b->getCell(ex, ey);
					if (c == 254) {
						highPixels++;
					} else if (c >= 1 && c <= 252) {
						pixels++;
						roughness += c;
					}
				}
			}
			if (pixels == 0) {
				if (highPixels == patchSize)
					setCell(hx, MOUNTAIN);	// alpine
				else if (t >= 6)
					setCell(hx, DEEP_SAND);	// corresponds to water - leave alone
				else if (isTransparent)
					setCell(hx, MARSH);
				continue;
			}
			if (pixels * 2 < patchSize && t == DEEP_WATER) {
				setCell(hx, DEEP_WATER);
				continue;
			} else if (t < 6)
				v = TUNDRA;
			else if (isTransparent)
				v = FOREST2;
			float r = float(roughness) / pixels;
			if (r >= highThreshold)
				setCell(hx, v | 0x30);
			else if (r >= middleThreshold)
				setCell(hx, v | 0x20);
			else if (r >= lowThreshold)
				setCell(hx, v | 0x10);
//				else if (r >= 12)
//					setCell(hx, DEEP_SAND | 0x10)
			else
				setCell(hx, v);
		}
	}
}
/*
    merge: (part: HexMap) 
	{
		for (r := 0; r < rows; r++){
			for (c := 0; c < cols; c++){
				i := r * cols + c
				if (part.data[i] != 0)
					data[i] = part.data[i]
			}
		}
    }
 */
float HexMap::hexToLatitude(xpoint hx) {
	double baseLatitude = header.latitude + header.height / 2;
	if ((header.rotation & 1) == 1)
		return baseLatitude - hx.x * header.height / _rowSize;
	else
		return baseLatitude - hx.y * header.height / _allocatedRows;
}

float HexMap::hexToLongitude(float latitude, xpoint hx) {
//		compression := cos(latitude * 3.14159 / 180)
	double compression = sqrt(90*90 - latitude * latitude) / 90;
	switch (header.rotation){
	case	0:
		return float(header.longitude + (hx.x - _rowSize / 2) * header.width / (_rowSize * compression));

	case	-1:
		return float(header.longitude + (_allocatedRows / 2 - hx.y) * header.width / (_allocatedRows * compression));

	case	1:
		return float(header.longitude + (hx.y - _allocatedRows / 2) * header.width / (_allocatedRows * compression));
	
	case	2:
		return float(header.longitude + (_rowSize / 2 - hx.x) * header.width / (_rowSize * compression));
	}
	return 0;
}

xpoint HexMap::latLongToHex(float lat0, float long0) {
	xpoint res;

	double baseLatitude = header.latitude + header.height / 2;
	double compression = sqrt(90*90 - lat0 * lat0) / 90;
	double ry = _allocatedRows * (baseLatitude - lat0) / header.height;
	int hy = (int)ry;
	double r2y = 2 * ry;
	int halfy = (int)r2y;
	double ixbase = (r2y - halfy) / 2;
	double rx = _rowSize / 2 + _rowSize * (long0 - header.longitude) * compression / header.width;
	int hx = (int)rx;
	double dx = rx - hx;
	if ((halfy & 1) == 0){
		intercepts[0] = (0.5f - ixbase) / 1.5f;
		intercepts[1] = ixbase / 1.5f;
		float ix = intercepts[hx & 1];
		if (dx > ix){
			ix = 1 + intercepts[1 - (hx & 1)];
			res.x = (xcoord)hx;
			if ((hx & 1) == 1){
				res.y = (xcoord)(hy - 1);
			} else {
				res.y = (xcoord)hy;
			}
		} else {
			res.x = (xcoord)(hx - 1);
			if ((hx & 1) == 0)
				res.y = (xcoord)(hy - 1);
			else
				res.y = (xcoord)hy;
		}
	} else {
		intercepts[1] = (0.5f - ixbase) / 1.5f;
		intercepts[0] = ixbase / 1.5f;
		float ix = intercepts[hx & 1];
		if (dx > ix)
			res.x = (xcoord)hx;
		else
			res.x = (xcoord)(hx - 1);
		res.y = (xcoord)hy;
	}
	return res;
}

display::point HexMap::latLongToPixel(float lat0, float long0, float zoom) {
	display::point res;

	double baseLatitude = header.latitude + header.height / 2;
	double compression = sqrt(90*90 - lat0 * lat0) / 90;
	double scale1 = 1.5f * zoom / sqrt3;
	res.y = ui::MAP_MARGIN + display::coord(zoom * _allocatedRows * (baseLatitude - lat0) / header.height);
	res.x = ui::MAP_MARGIN + display::coord(scale1 * (_rowSize / 2 + _rowSize * (long0 - header.longitude) * compression / header.width));
	return res;
}

float HexMap::hexScale() {
	double circumference = 40075.16;
	double klicksPerDegree = circumference / 360;
	if (header.height == 0)
		return 15;
	if ((header.rotation & 1) == 1)
		return header.height * klicksPerDegree / _rowSize;
	else
		return header.height * klicksPerDegree / _allocatedRows;
}

void HexMap::setCell(xpoint hx, int index) {
		if (index < 0 || index > 255)
			return;
		if (!valid(hx))
			return;

		Hex& h = hex(hx);
		h.data &= ~0xff;
		h.data |= index;
    }

void HexMap::setFeature(xpoint hx, ui::Feature* f) {
	if (!valid(hx))
		return;

	Hex& h = hex(hx);
	h.features = f;
}

void HexMap::addFeature(xpoint hx, ui::Feature* f) {
	if (!valid(hx))
		return;

	Hex& h = hex(hx);
	if (h.features != null)
		h.features->insert(f);
	else
		h.features = f;
}

void HexMap::hideFeature(xpoint p, ui::Feature *f) {
	ui::Feature* ftop = getFeature(p);
	if (ftop == f) {
		if (ftop->next() != ftop)
			setFeature(p, ftop->next());
		else
			setFeature(p, null);
	}
	f->pop();
}

int HexMap::getVictoryPoints(xpoint hex) {
	ui::Feature* f = getFeature(hex);
	if (f != null) {
		ui::Feature* sf = f;
		do {
			if (f->featureClass() == ui::FC_OBJECTIVE)
				return ((ui::ObjectiveFeature*)f)->value;
			f = f->next();
		} while (f != sf);
	}
	return 0;
}

ui::PlaceFeature* HexMap::getPlace(xpoint hx) {
	if (!valid(hx))
		return null;
	Hex& h = hex(hx);
	ui::Feature* fbase = h.features;
	if (fbase == null)
		return null;
	ui::Feature* fx = fbase;
	do {
		if (fx->featureClass() == ui::FC_PLACE) {
			return (ui::PlaceFeature*)fx;
		}
		fx = fx->next();
	} while (fx != fbase);
	return null;
}

float HexMap::getFortification(xpoint hx) {
	if (!valid(hx))
		return 0;
	Hex& h = hex(hx);
	ui::Feature* fbase = h.features;
	if (fbase == null)
		return 0;
	ui::Feature* fx = fbase;
	do {
		if (fx->featureClass() == ui::FC_FORTIFICATION){
			return ((ui::FortificationFeature*)fx)->level;
		}
		fx = fx->next();
	} while (fx != fbase);
	return 0;
}

void HexMap::setFortification(xpoint hx, float level) {
	if (!valid(hx))
		return;
	Hex& h = hex(hx);
	ui::Feature* fbase = h.features;

		// No features at all, make one

	if (fbase == null){
		if (level > 0)
			h.features = new ui::FortificationFeature(this, level);
		return;
	}

		// Look for a transport feature to extend

	ui::Feature* fx = fbase;
	do {
		if (fx->featureClass() == ui::FC_FORTIFICATION){
			if (level > 0)
				((ui::FortificationFeature*)fx)->level = level;
			else {
				hideFeature(hx, fx);
				delete fx;
			}
			return;
		}
		fx = fx->next();
	} while (fx != fbase);

		// No transport feature, put it after any units in 
		// the hex

	if (level > 0){
		fx = new ui::FortificationFeature(this, level);
		fbase->insert(fx);
	}
}

TransFeatures HexMap::getTransportEdge(xpoint hx, int e) {
	normalize(&hx, &e);
	if (!valid(hx))
		return TF_NONE;

	Hex& h = hex(hx);
	ui::Feature* fbase = h.features;
	if (fbase == null)
		return TF_NONE;
	ui::Feature* fx = fbase;
	do {
		if (fx->featureClass() == ui::FC_TRANSPORT){
			return ((ui::TransportFeature*)fx)->edge(e);
		}
		fx = fx->next();
	} while (fx != fbase);
	return TF_NONE;
}

void HexMap::setTransportEdge(xpoint hx, int e, TransFeatures f) {
	normalize(&hx, &e);
	if (!valid(hx))
		return;

	Hex& h = hex(hx);
	ui::Feature* fbase = h.features;
	if (fbase == null) {
		h.features = new ui::TransportFeature(this, e, f);
		return;
	}

	ui::Feature* fx = fbase;
	do {
		if (fx->setEdge(e, f))
			return;
		fx = fx->next();
	} while (fx != fbase);

		// No transport feature, put it after any units in 
		// the hex

	fx = new ui::TransportFeature(this, e, f);
	fbase->insert(fx);
}

void HexMap::clearTransportEdge(xpoint hx, int e, TransFeatures f) {
	normalize(&hx, &e);
	if (!valid(hx))
		return;

	Hex& h = hex(hx);
	ui::Feature* fbase = h.features;
	if (fbase == null)
		return;

	ui::Feature* fx = fbase;
	do {
		if (fx->clearEdge(e, f))
			return;
		fx = fx->next();
	} while (fx != fbase);
}

void HexMap::createDefaultBridges(const Theater* theater) {
	xpoint hx;

		// For every transport edge where there is a road or rail crossing the
		// hexside, and that hexside also has a river then construct a bridge 
		// there.  If the hexside also crosses the 'front line' blow the bridge.
		// This saves someone having to explicitly put bridges over every river
		// crossing.

	for (hx.x = _subsetOrigin.x; hx.x < _subsetOpposite.x; hx.x++)
		for (hx.y = _subsetOrigin.y; hx.y < _subsetOpposite.y; hx.y++) {
			Hex& h = hex(hx);
			int c = h.occupier;
			for (HexDirection dir = 0; dir < 3; dir++){
				TransFeatures tf = getTransportEdge(hx, HexDirection(dir));
				if (tf & (TF_RAIL|TF_ROAD|TF_PAVED|TF_FREEWAY)){
					EdgeValues e = edgeCrossing(hx, dir);
					if (e > EDGE_BORDER){
						setTransportEdge(hx, HexDirection(dir), TransFeatures(tf | TF_BRIDGE));
						int c2 = hex(neighbor(hx, HexDirection(dir))).occupier;
						if (theater->combatants[c] &&
							theater->combatants[c2] &&
							theater->combatants[c]->force != theater->combatants[c2]->force)
							setTransportEdge(hx, HexDirection(dir), TransFeatures(tf | TF_BRIDGE|TF_BLOWN_BRIDGE));
					}
				}
			}
		}
}

void HexMap::setEdge(xpoint hx, int e, EdgeValues ve) {
//		traceFile.writeLine("setEdge(" + x + "," + y + "," + e + "," + v + ")")
	if (!valid(hx))
		return;

	int v = int(ve);
	int mask = 3 << (8 + e * 2);
	v <<= (8 + e * 2);
	Hex& h = hex(hx);
	h.data &= ~mask;
	h.data |= v;
}

int HexMap::getCell(xpoint hx) {
	if (!valid(hx))
		return NOT_IN_PLAY;
	Hex& h = hex(hx);
	return h.data & 0xff;
}

ui::Feature* HexMap::getFeature(xpoint hx) {
	if (!valid(hx))
		return null;
	Hex& h = hex(hx);
	return h.features;
}

EdgeValues HexMap::getEdge(xpoint hx, int e) {
	if (!valid(hx))
		return EDGE_ERROR;
	Hex& h = hex(hx);
	int x = h.data;
	x >>= 8 + e * 2;
	return EdgeValues(x & 03);
}

float HexMap::getDefense(xpoint hx) {
	return terrainKey.table[getCell(hx) & 0xf].defense;
}

Detachment* HexMap::getDetachments(xpoint hx) {
	if (valid(hx))
		return hex(hx).detachments;
	else
		return null;
}

Combat* HexMap::combat(xpoint hx) const {
	if (valid(hx))
		return hex(hx).combat;
	else
		return null;
}

void HexMap::set_combat(xpoint hx, Combat* combat) {
	if (valid(hx))
		hex(hx).combat = combat;
}

/*
 *	FUNCTION:	enemyZoc
 *
 *	This function returns true if the hex in question is in an enemy
 *	zone of control.  Where the enemy is any other force than the one
 *	given.
 */
bool HexMap::enemyZoc(Force* force, xpoint hx) {
	for (HexDirection i = 0; i < 6; i++) {
		EdgeValues e = edgeCrossing(hx, i);
		if (e == EDGE_COAST)
			continue;
		Detachment* d = getDetachments(neighbor(hx, i));
		if (d != null && d->unit->opposes(force) && d->exertsZOC())
			return true;
	}
	return false;
}
/*
 *	FUNCTION:	enemyContact
 *
 *	This function returns true if the hex in question is adjacent to an enemy
 *	unit.  Where the enemy is any other force than the one
 *	given.
 */
bool HexMap::enemyContact(Force* force, xpoint hx) {
	for (HexDirection i = 0; i < 6; i++) {
		EdgeValues e = edgeCrossing(hx, i);
		if (e == EDGE_COAST)
			continue;
		Detachment* d = getDetachments(neighbor(hx, i));
		if (d != null && d->unit->opposes(force))
			return true;
	}
	return false;
}
/*
 *	FUNCTION: isFriendly
 *
 *	This function returns true if the hex is friendly to the given force
 */
bool HexMap::isFriendly(Force* force, xpoint hx) {
	return force == force->game()->theater()->combatants[getOccupier(hx)]->force;
}

bool HexMap::attackingFrom(xpoint loc1, xpoint loc2) {
	Combat* c = combat(loc2);
	if (c != null)
		return c->attackers.attackingFrom(loc1);
	else
		return false;
}

void HexMap::remove(Detachment* detachment) {
	Detachment* prev = null;
	Hex& h = hex(detachment->location());
	for (Detachment* d = h.detachments; d != null; prev = d, d = d->next)
		if (d == detachment){
			if (prev == null)
				h.detachments = d->next;
			else
				prev->next = d->next;
			break;
		}
}

void HexMap::place(Detachment* d) {
	d->next = null;
	Hex& h = hex(d->location());
	if (h.detachments == null)
		h.detachments = d;
	else {
		for (Detachment* dd = h.detachments; ; dd = dd->next) {
			if (dd->next == null) {
				dd->next = d;
				break;
			}
		}
	}
}

void HexMap::placeOnTop(Detachment* d) {
	Hex& h = hex(d->location());
	d->next = h.detachments;
	h.detachments = d;
}

void HexMap::setOccupier(xpoint hx, int index) {
	if (!valid(hx))
		return;
	Hex& h = hex(hx);
	h.occupier = index;
}

int HexMap::getOccupier(xpoint hx) {
	if (!valid(hx))
		return 0;
	Hex& h = hex(hx);
	return h.occupier;
}

float HexMap::getDensity(xpoint hx) {
	return terrainKey.table[getCell(hx) & 0xf].density;
}
/*
	getRoughness: (hx: xpoint) int
	{
		return (getCell(hx.x, hx.y) >> 4) & 0x3
	}
/*
	getElevation: (hx: xpoint) int
	{
		return (getCell(hx.x, hx.y) >> 6) & 0x3
	}
 */
float HexMap::getRoughDefense(xpoint hx) {
	int r = (getCell(hx) >> 4) & 0x3;
	int e = (getCell(hx) >> 6) & 0x3;
	r = r + e;
	if (r > 3)
		r = 3;
	return terrainKey.roughModifier[r].defense;
}

float HexMap::getRoughDensity(xpoint hx) {
	int r = (getCell(hx) >> 4) & 0x3;
	int e = (getCell(hx) >> 6) & 0x3;
	r = r + e;
	if (r > 3)
		r = 3;
	return terrainKey.roughModifier[r].density;
}

bool HexMap::hasRails(xpoint hx) {
	for (HexDirection i = 0; i < 6; i++) {
		if ((getTransportEdge(hx, i) & (TF_RAIL|TF_DOUBLE_RAIL|TF_TORN_RAIL)) == TF_RAIL)
			return true;
	}
	return false;
}

int HexMap::getRows() {
	return header.rows;
}

int HexMap::getColumns() {
	return header.cols;
}
/*
    getColumns: () int 
	{
		return cols
    }

	transferFrom: (h: HexMap)
	{
		hx: xpoint
		for (hx.y = 0; hx.y < rows; hx.y++){
			for (hx.x = 0; hx.x < cols; hx.x++){
				if (hx.x > h.cols || hx.y > h.rows){
					setCell(hx, 0)
					continue
				}
				n := hx.y * h.cols + hx.x
				v := h.data[n]
				c := v & 0x3f
				v &= 0xff00						// preserve the edges
				v |= translate[c]
				if (c >= 16 && c <= 56)
					v |= (c & 3) << 4
				else if (c == 13)
					v |= 0x10
				n = (hx.y + baseY) * rowSize + (hx.x + baseX)
				data[n] = v
			}
		}
	}
 */

#if 0
}

adjacentx: [8] int = [ 0, -1, 1, 0, -1, -1, 1, 1 ]
adjacenty: [8] int = [ -1, 0, 0, 1, -1, 1, -1, 1 ]
/*
translate: [64] byte = [
	0, 0, 1, 15,
	2, 0, 0, 0,
	0, 0, 0, 0,
	5, 13,0, 0,
	6, 6, 6, 6,
	7, 7, 7, 7,
	8, 8, 8, 8,
	9, 9, 9, 9,
	10,10,10,10,
	11,11,11,11,
	12,12,12,12,
	13,13,13,13,
	14,14,14,14,
	4, 4, 4, 4,
	0, 0, 0, 0,
	0, 0, 0, 0,
	]

    static Color capacities[];

    static {
	int i;

	capacities = new Color[64];

	for (i = 0; i < 48; i++)
		capacities[63 - i] = new Color(255 - i * 2, 0, 0);
	for (; i < 64; i++)
		capacities[63 - i] = new Color(303 - i * 3, 0, 0);
    }

    PopulationMap people;

    public void setPeople(PopulationMap p) {
	people = p;
    }

    public Color getColor(int x, int y) {
	if (x < 0 ||
	    x >= cols ||
	    y < 0 ||
	    y >= rows)
		return Color.black;
	if (people != null){
		Color c = people.getColor(x, y);

		if (c != null)
			return c;
	}
	if (showMode == 0)
		return table[data[y][x] & 0x3f];
	else if (showMode == 1){
		int i = data[y][x];
		int j = i;

		i &= 0x3f;
		if (i == 2)
			return table[2];

		int cap = farmCapacity[i];

		j >>= 8;
		while (j > 0){
			int k = j & 3;

			cap += farmBoost[k][i];
			j >>= 2;
		}

		if (y > 0){
			j = (data[y - 1][x] >> 12) & 3;

			cap += farmBoost[j][i];
		}
		if (x > 0){
			if ((x & 1) == 0){
				if (y > 0){
					j = (data[y - 1][x - 1] >> 10) & 3;

					cap += farmBoost[j][i];
				}
				j = (data[y][x - 1] >> 8) & 3;

				cap += farmBoost[j][i];
			} else {
				if (y < rows - 1){
					j = (data[y + 1][x - 1] >> 8) & 3;

					cap += farmBoost[j][i];
				}
				j = (data[y][x - 1] >> 10) & 3;

				cap += farmBoost[j][i];
			}
		}

		int lat = y - EQUATOR;

		if (lat < 0)
			lat = -lat;

		cap = (int)(cap * (POLE - lat) / (double)POLE);
		if (cap <= 0)
			return Color.black;
		cap /= 14;
		if (cap <= 0)
			return Color.black;
		if (cap >= capacities.length)
			cap = capacities.length - 1;
		return capacities[cap];
	} else if (showMode == 2){
		int i = data[y][x];
		int j = i;

		i &= 0x3f;
		if (i == 2)
			return table[2];

		int cap = nomadCapacity[i];

		j >>= 8;
		while (j > 0){
			int k = j & 3;

			cap += nomadBoost[k][i];
			j >>= 2;
		}

		if (y > 0){
			j = (data[y - 1][x] >> 12) & 3;

			cap += nomadBoost[j][i];
		}
		if (x > 0){
			if ((x & 1) == 0){
				if (y > 0){
					j = (data[y - 1][x - 1] >> 10) & 3;

					cap += nomadBoost[j][i];
				}
				j = (data[y][x - 1] >> 8) & 3;

				cap += nomadBoost[j][i];
			} else {
				if (y < rows - 1){
					j = (data[y + 1][x - 1] >> 8) & 3;

					cap += nomadBoost[j][i];
				}
				j = (data[y][x - 1] >> 10) & 3;

				cap += nomadBoost[j][i];
			}
		}

		int lat = y - EQUATOR;

		if (lat < 0)
			lat = -lat;

		cap = (int)(cap * (POLE - lat) / (double)POLE);
		if (cap <= 0)
			return Color.black;
		cap = (int)(cap * 0.8);
		if (cap >= capacities.length)
			cap = capacities.length - 1;
		return capacities[cap];
	} else
		return Color.black;
    }

 */

/*
TransFeatureIndexes: type enum[8] (
	TFI_BRIDGE,
	TFI_CLOGGED,
	TFI_ROAD,
	TFI_PAVED,
	TFI_FREEWAY,
	TFI_RAIL,
	TFI_TORN_RAIL,
	TFI_RAIL_CLOGGED,
	TFI_BLOWN_BRIDGE,
)
 */
/*
swapEnds: (x: int) int
{
	y := (x & 0xff) << 24
	y |= (x & 0xff00) << 8
	y |= (x & 0xff0000) >> 8
	y |= (unsigned(x) & 0xff000000) >> 24
	return y
}
 */
vertexChain:	Vertex
freeVertex:		Vertex

Vertex: type {
	next:		Vertex
	p:			xpoint
	vertex:		int						// zero is upper left, one is upper right

	new: (p: xpoint, v: int)
	{
		this.p = p
		this.vertex = v
	}
}

distance: (x0: float, y0: float, x1: float, y1: float) float
{
	deltax := x0 - x1
	deltay := y0 - y1
	return float(sqrt(deltax * deltax + deltay * deltay))
}

#endif

void normalize(xpoint* hx, int* edge) {

		// Normalize the coordinates so that e < 3

	switch (*edge) {
	case 5:
		hx->y--;
		*edge = 2;
		break;

	case 4:
		if ((hx->x & 1) == 0)
			hx->y--;
		hx->x--;
		*edge = 1;
		break;

	case 3:
		if ((hx->x & 1) != 0)
			hx->y++;
		hx->x--;
		*edge = 0;
		break;
	}
}

HexMap* loadHexMap(const string& filename, const string& placesFile, const string& terrainKeyFile) {
	static dictionary<HexMap*> maps;

	HexMap*const* mapP = maps.get(filename);
	if (*mapP != null)
		return *mapP;
	if (!fileSystem::exists(filename))
		return null;
	HexMap* map = new HexMap(filename, placesFile, terrainKeyFile, 0, 0);
	if (map->load()) {
		maps.insert(filename, map);
		return map;
	}
	delete map;
	return null;
}

}  // namespace engine