#pragma once
/*
	Europa Game Engine

	The code is divided into engine code, which defines the game model independently of the UI used
	to visualize it.  This is intended to make it easier to reconfigure the game into a client-server
	real-time game, for example.  In such a configuration, the engine code would run on a server and
	serialize the relevant parts of the game state for each UI.  That way, the client should not see
	private data they should not see.

	The engine state is modeled as a set of loaded files, each of which define some game component,
	such as a TOE, a map, or a weapons list.  Each file is parsed into some data structure, and then
	that structure is mapped into a larger universe of game objects.

	The engine can function inside an editing environment.  It should be possible to edit and re-constitute
	the compiled state of the game objects without leaking memory or crashing beause of stale pointers.
	Also, the process of loading the game objects is accelerated by using multiple threads to read
	and parse different files in parallel.

	The normal function for the engine is to load and run a game.

	The way this is accomplished is in principle the same regardless of the user-level function the
	process is running to do.

	A single global variable 'fileWeb' is the set of files and all their derived objects, encoding
	the dependencies amongst these structures (some structure only parse when other structures are
	in place).

	After each user input event, the web is made current, which will asynchronously load files from
	disk, possibly in parallel, parse formatted files into internal structures (xmp files to HexMap, for
	example), and then the UI is notified that given data structures are ready for display.
 */
#include "../common/data.h"
#include "../common/derivative.h"
#include "../common/dictionary.h"
#include "../common/machine.h"
#include "../common/script.h"
#include "../common/vector.h"
#include "../display/text_edit.h"
#include "basic_types.h"
#include "constants.h"

namespace display {

class Color;

}  // namespace display

namespace xml {

class Document;
class Element;
struct saxString;

}  // namespace xml

namespace ui {

class Feature;
class PlaceFeature;

}  // namespace ui

namespace engine {

class BadgeInfo;
class BitMap;
class Colors;
class Combat;
class Combatant;
class Deployment;
class Detachment;
class Doctrine;
class Equipment;
class Force;
class ForceDefinition;
class Game;
class GameEvent;
class HexMap;
class IssueEvent;
class Operation;
class OrderOfBattle;
class PathHeuristic;
class Scenario;
class Section;
class Segment;
class StandingOrder;
class SupplyDepot;
class Theater;
class TheaterFile;
class Toe;
class ToeSet;
class Unit;
class UnitDefinition;
class Weapon;

extern const char* unitCarrierNames[];

UnitCarriers lookupCarriers(const xml::saxString& s);

extern const char* unitModeNames[];
extern const char* unitModeMenuNames[];

UnitModes toUnitMode(const xml::saxString& s);

extern data::Boolean logToggle;

bool logging();
void referenceGame(Game* game);
void logToBuffer(display::TextBuffer* buffer);
void setupLog();
void changeLogging();
void openLog();
void logToConsole();
void closeLog();
void log(const string& s);
void logPrintf(const char* format, ...);
void logSeparator();
string logGameTime(minutes t);

float parseDegrees(const string& s);
float parseDegrees(const xml::saxString& s);

unsigned hexAttribute(const xml::saxString& a);

void decodeFortsData(HexMap* map, const char* data, int length);

string encodeFortsData(HexMap* map);

void writeStrip(string* m, int cell, int count);

bool loadTOE(ToeSet* toeSet, display::TextBufferSource* source, const Theater* theater);

bool loadOOB(OrderOfBattle* oob, display::TextBufferSource* source, const Theater* theater);

void initSectionMap();

int sizeIndex(const char* size, int length);

const int SZ_NO_SIZE = -1;
const int SZ_PLATOON = 0;
const int SZ_COMPANY = 1;
const int SZ_REGIMENT = 3;
const int SZ_CORPS = 6;
const int SZ_ARMY = 7;

extern string unitSizes[];

extern const char* unitSizeNames[];

enum SectionFlags {
	SF_BADGE_PRESENT	= 0x01,
	SF_SIZE_PRESENT		= 0x02,
};

void writeOptionalString(FILE* out, const string& label, const string& s);

class Equipment {
	Equipment();
public:
	Equipment(Weapon* w, int a);

	static Equipment* factory(fileSystem::Storage::Reader* r);

	void store(fileSystem::Storage::Writer* o) const;

	bool equals(const Equipment* e) const;

	Equipment*	next;
	int			onHand;
	int			authorized;
	Weapon*		weapon;
};
/*
	This function examines a unit pattern string and returns an
	ordinal value to use for composing the unit name.
	
	If it is a positive integer the function returns that integer.

	If it ends with a slash followed by a positive integer, it 
	returns the negative of that integer.

	Note: the negative value designates that the pattern string
	needs to be used and cannot be mapped to roman numerals.
 */
int extractOrdinal(const string& pattern);
/*
 *	This function maps a designation string from a scenario or
 *	TO&E file into a name string.  The unitDes is the des attribute
 *	on a unit tag. The toeDes is the des attribute from the unit's
 *	toe.  The toeDes is a pattern which is completed by the unitDes.
 *
 *	A pattern can contain text or the following formatting characters:
 *
 *		#	Replace with an ordinal string (1st, 2nd, etc.)
 *
 *		@	Replace with a Roman numeral string (I, XV, etc.)
 *
 *		%	Replace with a 'regimental' ordinal (e.g. company number within regiment)
 *
 *		*	Replace with parent abbreviation/name
 *
 *	If no pattern character exists in the toe pattern, the unit designator
 *	string is simply prefixed to the toe pattern.
 */
string mapDesignation(const string& unitDes, const string& toeDes);
/*
 *	This function will produce a Roman numeral (new style) string
 *	for any unsigned value.  Note that for very large numbers, the
 *	number of M's becomes prohibitively large (and generating the string prohibitively
 *	expensive), so use this routine primarily for smallish numbers (<10,000).
 */
string romanNumeral(unsigned u);

extern Event1<Unit*> unitChanged;
extern Event2<Unit*, Unit*> unitAttaching;
/*
 *	updateUi
 *
 *	This is fired after each game event is processed to give the UI
 *	an opportunity to re-paint any parts of the window that have changed.
 */
extern Event updateUi;

void decodeCountryData(HexMap* map, const char* data, int length);

string encodeCountryData(HexMap* map);

display::TextBufferManager* manageTextFile(const string& filename);

void printErrorMessages();

void getTextFiles(vector<display::TextBufferManager*>* output);

template<class T>
void manageFile(const string& filename, dictionary<T*>* catalog);

display::TextBufferSource* textFileSource(const string& filename);

bool textFilesNeedSave();

bool saveTextFiles();

void writeFilenameAttribute(FILE* fp, const char* attributeName, const string& filename, const string& baseFilename);

UnitModes detachedMode(UnitModes parentMode, BadgeRole childRole);

void initTestObjects();

}  // namespace engine
