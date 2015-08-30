#include "../common/platform.h"
#include "path.h"

#include "../test/test.h"
#include "game_map.h"

namespace engine {

StraightPath straightPath;
SupplyPath supplyPath;
DepotPath depotPath;
UnitPath unitPath;

//	
// I use some pointers for 'efficiency'
//
// Modified to convert to C++ and integrate into
// Europa.
//
// The original code makes extensive use of templates and some
// container classes that I've transposed into thise code body.
// I also converted the code to use the map types of Europa.
//
// Lastly, there is the occasional assumption being made in the
// original code about the underlying game mechanics of Amit's
// surrounding code.  This has been modified to fit into the
// Europa architecture.
//
// Modifications have been more extensive that I first thought.  
// I've got the code working, but it's resemblance to Amit's
// original code is strained.  The path finding code is now just
// a single function, not a class.  I've also had to ditch most
// of Amit's unit pathfinding code, since it relied on his terrain
// model.  I've had to substitute my own terrain model.
//
// I've also chosen to limit the code to a single thread at a time.
// Repeat: this is not thread-safe code.  The OPEN set is maintained
// in a Container object that is global to the process.
//////////////////////////////////////////////////////////////////////
// Amit's Path-finding (A*) code.
//
// Copyright (C) 1999 Amit J. Patel
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies and
// that both that copyright notice and this permission notice appear
// in supporting documentation.  Amit J. Patel makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied warranty.
//
//
// This code is not self-contained.  It compiles in the context of
// my game (SimBlob) and will need modification to work in another
// program.  I am providing it as a base to work from, and not as
// a finished library.
//
// The main items of interest in my code are:
// 
// 1. I'm using a hexagonal grid instead of a square grid.  Since A*
//    on a square grid works better with the "Manhattan" distance than with
//    straight-line distance, I wrote a "Manhattan" distance on a hexagonal
//    grid.  I also added in a penalty for paths that are not straight
//    lines.  This makes lines turn out straight in the simplest case (no
//    obstacles) without using a straight-line distance function (which can
//    make the path finder much slower).
//
//    To see the distance function, search for UnitMovement and look at
//    its 'dist' function.
//
// 2. The cost function is adjustable at run-time, allowing for a
//    sort of "slider" that varies from "Fast Path Finder" to "Good Path
//    Quality".  (My notes on A* have some ways in which you can use this.)
//
// 3. I'm using a data structure called a "heap" instead of an array
//    or linked list for my OPEN set.  Using lists or arrays, an
//    insert/delete combination takes time O(N); with heaps, an
//    insert/delete combination takes time O(log N).  When N (the number of
//    elements in OPEN) is large, this can be a big win.  However, heaps
//    by themselves are not good for one particular operation in A*.
//    The code here avoids that operation most of the time by using
//    a "Marking" array.  For more information about how this helps
//    avoid a potentially expensive operation, see my Implementation
//    Nodes in my notes on A*.
//
//  Thanks to Rob Rodrigues dos santos Jr for pointing out some
//  editing bugs in the version of the code I put up on the web.
//////////////////////////////////////////////////////////////////////

/*
	may 19, 2001: The above comment has been retained to reflect the original
	aothorship of the code, but it has been extensively reworked.
 */
/*
	This algorithm, derived from the A* pathing code, performs a complete tour of all hexes around
	a point, more or less in order of the shortest distance traveled from the origin.  This way, we 
	can use this algorithm to find paths, fill fields of influence, find nearest interesting points, 
	distribute supplies, etc.
 */
class Node {
public:
	Node*		prev;
	Node*		next;

    xpoint		h;				// location on the map, in hex coordinates
    int			gval;			// g in A* represents how far we've already gone
    int			hval;			// h in A* represents an estimate of how far is left

	Node() {
		prev = null;
		next = null;
		h.x = 0;
		h.y = 0;
		gval = 0;
		hval = 0;
	}
	
	void init(xpoint h, int hval, int gval) {
		this->h = h;
		this->hval = hval;
		this->gval = gval;
	}

	bool lessThan(Node* n) {
		return gval + hval < n->gval + n->hval;
	}
};
// The mark array marks directions on the map.  The direction points
// to the spot that is the previous spot along the path.  By starting
// at the end, we can trace our way back to the start, and have a path.
// It also stores 'f' values for each space on the map.  These are used
// to determine whether something is in OPEN or not.  It stores 'g'
// values to determine whether costs need to be propagated down.
struct Marking {
    HexDirection	direction;	// !DirNone means OPEN || CLOSED
	Node*			n;			// != null means OPEN
};

inline int xpointToIndex(HexMap* map, xpoint hx) {
	return hx.y * map->getColumns() + hx.x;
}

class Container {
public:
	Node*			first;
	Node*			last;

	Node* getFirstNode() {
		if (first == null)
			return null;
		Node* n = first;
		first = n->next;
		if (first != null)
			first->prev = null;
		else
			last = null;
		return n;
	}

	void insert(Node* N) {
		insert(N, last);
	}

	void insert(Node* N, Node* n) {
		for (; n != null; n = n->prev)
			if (n->lessThan(N))
				break;
		N->prev = n;
		if (n != null){
			N->next = n->next;
			n->next = N;
		} else {
			N->next = first;
			first = N;
		}
		if (N->next != null)
			N->next->prev = N;
		else
			last = N;
	}

	void append(Node* N) {
		if (first == null){
			N->next = null;
			N->prev = null;
			first = N;
			last = N;
		} else {
			last->next = N;
			N->prev = last;
			N->next = null;
			last = N;
		}
	}

	void recalc(Node* N) {
		if (N->prev == null)
			return;					// already at the front of the heap
		if (N->next != null)
			N->next->prev = N->prev;
		else
			last = N->prev;
		N->prev->next = N->next;
		insert(N, N->prev);
	}

	void clear(HexMap* map, Node* N);
		
	void clear(HexMap* map);
};

static Marking* mark;
static Container open, freeNodes, visited;

void Container::clear(HexMap* map, Node* N) {
	mark[xpointToIndex(map, N->h)].direction = DirNone;
	append(N);
}
	
void Container::clear(HexMap* map) {
	    // Erase the mark array, for all items in open
	Node* n = first;
	while (n != null){
		Node* nNext = n->next;
		freeNodes.clear(map, n);
		n = nNext;
	}
	first = null;
	last = null;
}

static int nodeCount = 0;

static Node* newNode(xpoint p, int h, int g) {
	Node* n = freeNodes.getFirstNode();
	if (n == null){
		nodeCount++;
		n = new Node();
	}
	n->init(p, h, g);
	open.insert(n);
	return n;
}

static void propagate_down(HexMap* map, PathHeuristic* heuristic, Node* H);

void visit(HexMap* map, PathHeuristic* path, engine::xpoint A, int maxDist, SegmentKind kind) {
	if (mark == null) {
		int cells = map->getRows() * map->getColumns();
		mark = new Marking[cells];
		for (int i = 0; i < cells; i++)
			mark[i].direction = DirNone;
	}

		// insert the original node

	Node* N = newNode(A, 0, 0);
	mark[xpointToIndex(map, A)].n = N;
	mark[xpointToIndex(map, A)].direction = DirStart;

	int nodesRemoved = 0;

	// * Things in OPEN are in the open container (which is a heap),
	//   and also their mark[...].f value is nonnegative.
	// * Things in CLOSED are in the visited container (which is unordered),
	//   and also their mark[...].direction value is not DirNone.

	// While there are still nodes to visit, visit them!
	for (;;) {
		N = open.getFirstNode();
		if (N == null)
			break;
		xpoint h = N->h;
		mark[xpointToIndex(map, h)].n = null;
		PathContinuation result = path->visit(h);
		if (result == PC_STOP_ALL)
			break;

		nodesRemoved++;
		if (nodesRemoved > path->visitLimit)
			break;

		visited.append(N);

		if (result == PC_STOP_THIS)
			continue;

		// Look at your neighbors.
		for(HexDirection d = 0; d < 6; ++d) {
			xpoint hn = neighbor(h, d);
			// If it's off the end of the map, then don't keep scanning
			if(!map->valid(hn))
				continue;

			int k = N->gval + path->kost(h, d, hn);
			if (k >= maxDist)
				continue;

				// If this spot (hn) hasn't been visited, its mark is DirNone
			Marking& m = mark[xpointToIndex(map, hn)];
			if (m.direction == DirNone) {
				// The space is not marked

				Node* N2 = newNode(hn, 0, k);
				m.direction = reverseDirection(d);
				m.n = N2;
			}
				// We know it's in OPEN or VISITED...
			else if (m.n != null) {
				// It's in OPEN
				Node* find1 = m.n;

				// It's in OPEN, so figure out whether g is better
				if (k < find1->gval) {
					// Search for hn in open

            
					// Replace *find1's gval with N2.gval in the list&map
					m.direction = reverseDirection(d);
					find1->gval = k;
					open.recalc(find1);

					// This next step is not needed for most games

					propagate_down(map, path, find1);
				}
			}
		}
	}
	path->finished(map, kind);
	if (N != null)
		freeNodes.clear(map, N);
	open.clear(map);
	visited.clear(map);
}
/*
	This Path heuristic will find the shortest distance between A and B by using visit with
	a STOP_ALL when the right path is found.
 * /
StraightPath: type inherits PathHeuristic {
	destination:	xpoint
	foundIt:		boolean
	path:			pointer Segment
 */
Segment* StraightPath::find(HexMap* map, xpoint A, xpoint B, SegmentKind kind) {
	source = A;
	destination = B;
	foundIt = false;
	visitLimit = 1000;
	engine::visit(map, this, A, map->getRows() + map->getColumns(), kind);
	return path;
}

int StraightPath::kost(xpoint a, HexDirection dir, xpoint b) {
    double dx1 = double(a.x - b.x);
	double dy1 = double(a.y - b.y);
	double dd1 = dx1 * dx1 + dy1 * dy1;

		// The cross product will be high if two vectors are not colinear
		// so we can calculate the cross product of [current->goal] and
		// [source->goal] to see if we're staying along the [source->goal]
		// vector.  This will help keep us in a straight line.

	double dx2 = double(source.x - b.x);
	double dy2 = double(source.y - b.y);
	double cross = dx1 * dy2 - dx2 * dy1;

    if (cross < 0) 
		cross = -cross;
    return int(dd1 + cross);
}

PathContinuation StraightPath::visit(xpoint h) {
	if (h.x == destination.x && 
		h.y == destination.y){
		foundIt = true;
		return PC_STOP_ALL;
	} else
		return PC_CONTINUE;
}

void StraightPath::finished(HexMap* map, SegmentKind kind) {
	path = null;
	if (foundIt){

		// We have found a path, so let's copy it into `path'

		xpoint h = destination;
		while( h.x != source.x || h.y != source.y ){
			HexDirection dir = mark[xpointToIndex(map, h)].direction;
			xpoint hn = neighbor(h, dir);
			Segment* s = new Segment;
			s->next = path;
			s->nextp = h;
			s->hex = hn;
			s->dir = dir;
			s->kind = kind;
			s->cost = kost(hn, reverseDirection(dir), h);
			h = hn;
			path = s;
		}
		// path now contains the hexes in which the unit must travel ..
		// backwards (like a stack)
	}
}

int PathHeuristic::kost(xpoint a, HexDirection dir, xpoint b) {
	return 0;
}

PathContinuation PathHeuristic::visit(xpoint a) {
	return PC_CONTINUE;
}

void PathHeuristic::finished(HexMap* map, SegmentKind kind) {
}

void PathHeuristic::review(HexMap* map) {
	for (Node* v = visited.first; v != null; v = v->next)
		reviewHex(map, v->h, v->gval);
}

void PathHeuristic::reviewHex(HexMap* map, xpoint a, int distance) {
}
/*
//ALTITUDE_SCALE: const int = (NUM_TERRAIN_TILES/16)
MAXIMUM_PATH_LENGTH: const int = 1000000000		// distances are in 'minutes'

// Path_div is used to modify the heuristic.  The lower the number,
// the higher the heuristic value.  This gives us worse paths, but
// it finds them faster.  This is a variable instead of a constant
// so that I can adjust this dynamically, depending on how much CPU
// time I have.  The more CPU time there is, the better paths I should
// search for.
//path_div: int = 6

*/
HexDirection reverseDirection(HexDirection d) {
    // With hexagons, I'm numbering the directions 0 = N, 1 = NE,
    // and so on (clockwise).  To flip the direction, I can just
    // add 3, mod 6.
    return HexDirection( ( 3+int(d) ) % 6 );
}

xpoint neighbor(xpoint p, HexDirection d) {
	switch (d){
	case	0:
		p.y--;
		break;

	case	1:
		if ((p.x & 1) == 0)
			p.y--;
		p.x++;
		break;

	case	2:
		if ((p.x & 1) != 0)
			p.y++;
		p.x++;
		break;

	case	3:
		p.y++;
		break;

	case	4:
		if ((p.x & 1) != 0)
			p.y++;
		p.x--;
		break;

	case	5:
		if ((p.x & 1) == 0)
			p.y--;
		p.x--;
		break;
	}
	return p;
}

HexDirection directionTo(xpoint a, xpoint b) {
	if (a.x == b.x) {
		if (a.y < b.y)
			return 2;
		else
			return 5;
	}
	if (a.x < b.x) {
		if ((a.x & 1) == 0) {
			if (a.y > b.y)
				return 0;
			else
				return 1;
		} else {
			if (a.y >= b.y)
				return 0;
			else
				return 1;
		}
	} else {
		if ((a.x & 1) == 0) {
			if (a.y > b.y)
				return 4;
			else
				return 3;
		} else {
			if (a.y >= b.y)
				return 4;
			else
				return 3;
		}
	}
}

// This is the 'propagate down' stage of the algorithm, which I'm not
// sure I did right. BJ: - Looks right to me.  Of course, I've re-written
// it to be recursive, so who knows whether the original code worked.

void propagate_down(HexMap* map, PathHeuristic* heuristic, Node* H) {

    // Examine its neighbors
    for (HexDirection d = 0; d < 6; ++d) {
        xpoint hn = neighbor(H->h, d);
		if (!map->valid(hn))
			continue;
		Marking& m = mark[xpointToIndex(map, hn)];
        if (m.direction != DirNone && m.n != null) {
            // This node is in OPEN                
			int new_g = H->gval + heuristic->kost(H->h, d, hn);

            // Compare this `g' to the stored `g' in the array
			Node* n = m.n;
            if (new_g < n->gval) {
                // Push this thing UP in the heap (only up allowed!)
                n->gval = new_g;
				open.recalc(n);

                // Set its direction to the parent node

                m.direction = reverseDirection(d);
				propagate_down(map, heuristic, n);
            } else {
                // The new node is no better, so stop here
            }
        } else {
            // Either it's in closed, or it's not visited yet
        }
    }
}

int hexDistance(xpoint a, xpoint b) {
        // The **Manhattan** distance is what should be used in A*'s heuristic
        // distance estimate, *not* the straight-line distance.  This is because
        // A* wants to know the estimated distance for its paths, which involve
        // steps along the grid.  (Of course, if you assign 1.4 to the cost of
        // a diagonal, then you should use a distance function close to the
        // real distance.)

        // Here I compute a ``Manhattan'' distance for hexagons.  Nifty, eh?
	int dx = abs(b.x - a.x);
	int dy = b.y - a.y;
	if ((dx & 1) != 0) {
		if ((a.x & 1) == 1)
			dy--;
		if (dy < 0)
			dy = -dy - 1;
	} else if (dy < 0)
		dy = -dy;
	return dx + dy - (dx >> 1);
}
/*
max: private (a: int, b: int) int
{
	if (a > b)
		return a
	else
		return b
}
*/
Segment::~Segment() {
	if (next)
		delete next;
}

Segment* Segment::factory(fileSystem::Storage::Reader* r) {
	Segment* s = new Segment();
	int kind;
	if (r->read(&s->hex.x) &&
		r->read(&s->hex.y) &&
		r->read(&s->nextp.x) &&
		r->read(&s->nextp.y) &&
		r->read(&s->dir) &&
		r->read(&kind) &&
		r->read(&s->cost) &&
		r->read(&s->next)) {
		s->kind = (SegmentKind)kind;
		return s;
	} else {
		delete s;
		return null;
	}
}


void Segment::store(fileSystem::Storage::Writer* o) const {
	o->write(hex.x);
	o->write(hex.y);
	o->write(nextp.x);
	o->write(nextp.y);
	o->write(dir);
	o->write((int)kind);
	o->write(cost);
	o->write(next);
}

bool Segment::equals(Segment* seg) const {
	if (hex.x == seg->hex.x &&
		hex.y == seg->hex.y &&
		nextp.x == seg->nextp.x &&
		nextp.y == seg->nextp.y &&
		dir == seg->dir &&
		kind == seg->kind &&
		cost == seg->cost &&
		test::deepCompare(next, seg->next))
		return true;
	else
		return false;
}

}  // namespace engine
