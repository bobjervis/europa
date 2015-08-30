#pragma once
#include <windows.h>
#include "../common/machine.h"
#include "../engine/basic_types.h"

namespace xml {

class Document;
class Element;

}  // namespace xml

namespace display {

class Bitmap;
class Device;

}  // namespace display

namespace engine {

const short BITMAP_MAGIC_NUMBER = short('B' + (int('M') << 8));

class HexMap;

class BitMap {
public:
	BitMap();

	explicit BitMap(byte* image);

	virtual ~BitMap() {}

	display::Bitmap* getDisplayBitmap(display::Device* b);

	virtual BitMap* dupHeader();

	virtual int getCell(int x, int y) = 0;

	virtual bool setColorPalette(int index, int color);

	virtual int getColorPalette(int index);

	string				filename;
	int					rows, cols;
	int					rowSize;
	int*				table;
	byte*				data;
	byte*				image;
	BITMAPINFO*			header;
	display::Bitmap*	dBitmap;
};

class BitMap4 : public BitMap {
	typedef BitMap super;
public:
	explicit BitMap4(byte* image, const string& filename);

	virtual int getCell(int x, int y);

	virtual bool setColorPalette(int index, int color);

	virtual int getColorPalette(int index);

private:
	int			maxColor;
};

class BitMap8 : public BitMap {
	typedef BitMap super;
public:
	BitMap8();

	explicit BitMap8(byte* image, const string& filename);

	virtual BitMap* dupHeader();

	virtual int getCell(int x, int y);

	virtual bool setColorPalette(int index, int color);

	virtual int getColorPalette(int index);

private:
	int			maxColor;
};

class BitMap24 : public BitMap {
	typedef BitMap super;
public:
	explicit BitMap24(byte* image, const string& filename);

	virtual int getCell(int x, int y);

};

class MapLocation {
public:
	MapLocation() {
		x = 0;
		y = 0;
	};

	string		name;
	int			x;
	int			y;
};

class ScaledBitMapData {
public:
	string		filename;
	MapLocation	loc1;
	MapLocation	loc2;
	string		colors;
};

class ScaledBitMap : public BitMap {
public:
	ScaledBitMap();

	ScaledBitMap(HexMap* xmp, const string& filename, int x1, int y1, int x2, int y2, const string& loc1,
												const string &loc2);

	ScaledBitMap(HexMap* xmp, const string& filename, xml::Element* e);

	void init(int x1, int y1, float lat1, float long1, int x2, int y2, float lat2, float long2);
	
	bool freshen(const string& filename, int x1, int y1, int x2, int y2, const string& loc1,
												const string &loc2);

	virtual int getCell(int x, int y);

	HexMap*				xmp;
	BitMap*				realMap;
	float				baseY;
	float				deltaY;
	float				baseLat;
	float				deltaLat;
	float				baseLong;
	float				deltaLong;
	ScaledBitMapData	data;
};

struct ComboBitMapItem {
	ComboBitMapItem*	next;
	BitMap*				realMap;
	int					ignoreIndex;
	int					clearIndex;
	int					colorBase;
};

class ComboBitMap : public ScaledBitMap {
public:
	ComboBitMap(xml::Document* doc, HexMap* xmp, const string& filename, xml::Element* e);

	virtual int getCell(int x, int y);

private:
	ComboBitMapItem*	componentMaps;
};

class LambertBitMap : public ScaledBitMap {
public:
	LambertBitMap(HexMap* xmp, const string& filename, xml::Element* e);

	virtual int getCell(int x, int y);

private:
	BitMap*			realMap;
	float			deltaLat;
	float			xmpBaseLat;
	float			xmpDeltaLat;
	float			baseLong;
	double			poleY;
	double			cosTheta1;
	double			sinTheta1;
	float			scale;
	float			halfRows;
	float			stretchX;
	float			lambda0Col;
};

const int PARC_WHITE = 0;
const int PARC_COAST = 1;
const int PARC_BORDER = 2;
const int PARC_BLUE = 3;
const int PARC_RIVER = 4;
const int PARC_DEPRESSION = 5;
const int PARC_GREY = 6;
const int PARC_WADI = 7;
const int PARC_CANAL = 8;
const int PARC_RED = 9;
const int PARC_GRID = 10;

const int PARC_XRIVER = 11;
const int PARC_XCOAST = 12;
const int PARC_XBORDER = 13;
/*
	ParcMap

	This is a bit map that has been downloaded from the Xerox/Parc
	Map server.  All such bitmaps are formatted as .GIF files.  These
	are described by a set of parameters:

		- Latitude and longitude of center point
		- size of area covered in degrees
		- size of bitmap
		- type of projection (elliptical, square)

	The GIF is then converted to a 256-color .BMP file.  The parameters
	describing the map allow for the normalization of this data into a
	'standard form'.  This will allow hex maps produced from this data to
	be specifiable in a simple manner, without having to have detailed knowledge
	of the source data in each case.

	The color map for such a file is always:

			index			meaning

			  0				white - most pixels
			  1				coast
			  2				border
			  3				deep blue - unused 
			  4				river
			  5				depression (outline)
			  6				grey - unused
			  7				wadi (seasonal water)
			  8				canal
			  9				red - unused
			  10			grey - grid lines

	These maps obey certain rules that can be used to automatically re-process them
	into a hex map.  The following is true of the data

	- pixels colored border, grid or white are not necessarily land pixels.
	- pixels colored river, wadi, canal or depression are always land.

	By convention, hand-touching the bitmaps is sometimes necessary.  In order to 
	guide the land identification algorithm, the lines are quite good in that the
	coast lines are almost always continuous (allowing for clean flood fills to 
	work properly).  The occasional break is unavoidable, however, so MS Paint can
	touch up the missing coast pixels.  Streams also sometimes have breaks, though 
	here, the hex river tracing algorithm is probably more stable (tending to produce 
	good river lines).

	- maps really should be done both with and without borders.  Borders obscure
	  river courses, so watch out.
	- Grid lines don't matter.  The query parameters should satisfy any scaling
	  or positioning needed, so grid lines have the exact same characteristics as
	  white pixels - could be either land or sea, it depends.

	Because the elevation and gradient maps also include water classification data for
	sea water at least, this can be used as a cross check, but is generally unnecessary.
	The elevation data is particularly poor at identifying lakes, so a good coast tracer
	is needed.

	Note: land classification can wait until transferring to the hex map.  But that tends
	to confuse the issue on extremely tangled coasts.  Or does it?
 */
class ParcMap {
public:
	ParcMap();

	ParcMap(const string& filename, float latitude, float longitude, float width, float height, bool isElliptical);

	virtual xpoint hexScale(float kilometersPerHex, int rotation);

	float			deltax;
	float			deltay;
	BitMap*			bitMap;
	float			latitude;
	float			longitude;
	float			width;
	float			height;
	int				originy;
	bool			isElliptical;
	string			colors;

	HexMap* map() const { return _map; }

private:
	HexMap*			_map;
};

class ScaledParcMap : public ParcMap {
public:
	explicit ScaledParcMap(ScaledBitMap* b);

	virtual xpoint hexScale(float kilometersPerHex, int rotation);
};

class RotatedBitMap : public BitMap {
public:
	BitMap*			bitMap;
	int				rotation;

	RotatedBitMap(BitMap* b, int rotation);

	virtual int getCell(int x, int y);

	virtual bool setColorPalette(int index, int color);

	virtual int getColorPalette(int index);
};

BitMap* loadBitMap(const string& filename);

}  // namespace engine
