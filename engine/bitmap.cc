#include "../common/platform.h"
#include "bitmap.h"

#include "../common/file_system.h"
#include "../common/xml.h"
#include "../display/device.h"
#include "engine.h"
#include "game_map.h"

namespace engine {

BitMap4::BitMap4(byte *image, const string& filename) : super(image) {
	this->filename = filename;
	if (header->bmiHeader.biClrUsed == 0)
		maxColor = 16;
	else
		maxColor = header->bmiHeader.biClrUsed;
	int bytesPerRow = (cols * header->bmiHeader.biBitCount + 7) >> 3;
	rowSize = (bytesPerRow + 3) & ~3;
}
/*
BitMap4: public type inherits BitMap {
	maxColor:		int

	new: (x: int, y: int)
	{
		super(x, y, 16)
		header.biBitCount = 4
		bytesPerRow := x * header.biBitCount >> 3
		rowSize = (bytesPerRow + 3) & ~3;
		header.biSizeImage = rowSize * y
		data = unmanaged new [header.biSizeImage] byte
		maxColor = 16
	}

	setCell: (x: int, y: int, index: int) 
	{
		if (index < 0 || index > 15)
			return
		if (x < 0 || x >= cols)
			return
		if (y < 0 || y >= rows)
			return

		cell := y * rowSize + (x >> 1)

		if ((x & 1) != 0){
			data[cell] &= ~0xf
			data[cell] |= index
		} else {
			data[cell] &= ~0xf0
			data[cell] |= index << 4
		}
    }
*/
int BitMap4::getCell(int x, int y) {
	if (x < 0 ||
		x >= cols ||
		y < 0 ||
		y >= rows)
		return -1;
	int v = data[y * rowSize + (x >> 1)];
	if ((x & 1) != 0)
		return v & 0x0F;
	else
		return (v >> 4) & 0x0F;
}
/*
	getColor: (x: int, y: int) int
	{
		i := getCell(x, y)
		if (i < 0 || i >= maxColor)
			return -1
		else
			return table[i]
	}
*/
bool BitMap4::setColorPalette(int index, int color) {
	if (index < maxColor && index >= 0){
		table[index] = color;
		return true;
	} else
		return false;
}


int BitMap4::getColorPalette(int index) {
	if (index < maxColor && index >= 0)
		return table[index];
	else
		return 0;
}

BitMap8::BitMap8() {
}

BitMap8::BitMap8(byte *image, const string& filename) : super(image) {
	this->filename = filename;
	if (header->bmiHeader.biClrUsed == 0)
		maxColor = 256;
	else
		maxColor = header->bmiHeader.biClrUsed;
	rowSize = (cols + 3) & ~3;
}
/*
	new: (x: int, y: int)
	{
		super(x, y, 256)
		header.biBitCount = 8
		rowSize = (x + 3) & ~3
		header.biSizeImage = rowSize * y
		data = unmanaged new [header.biSizeImage] byte
		maxColor = 256
	}
*/
BitMap* BitMap8::dupHeader() {
	BitMap8* b = new BitMap8();
	b->data = data;
	b->image = image;
	b->header = (BITMAPINFO*)calloc(1, sizeof (BITMAPINFOHEADER) + 256 * 4);
	*b->header = *header;
	b->table = (int*)((byte*)(b->header) + sizeof (BITMAPINFOHEADER));
	memcpy(b->table, table, 256 * 4);
	b->rows = rows;
	b->cols = cols;
	b->rowSize = rowSize;
	b->maxColor = 256;
	return b;
}
/*
	setCell: (x: int, y: int, index: int) 
	{
		if (index < 0 || index > 256)
			return
		if (x < 0 || x >= cols)
			return
		if (y < 0 || y >= rows)
			return
		data[y * rowSize + x] = byte(index)
    }
*/
int BitMap8::getCell(int x, int y) {
	if (x < 0 ||
		x >= cols ||
		y < 0 ||
		y >= rows)
		return -1;
	return data[y * rowSize + x];
}
/*
	getColor: (x: int, y: int) int
	{
		i := getCell(x, y)
		if (i < 0 || i >= maxColor)
			return -1
		else
			return table[i]
	}
*/
bool BitMap8::setColorPalette(int index, int color) {
	if (index < maxColor && index >= 0){
		table[index] = color;
		return true;
	} else
		return false;
}

int BitMap8::getColorPalette(int index) {
	if (index < maxColor && index >= 0)
		return table[index];
	else
		return 0;
}

BitMap24::BitMap24(byte *image, const string& filename) : super(image) {
	this->filename = filename;
	int bytesPerRow = cols * header->bmiHeader.biBitCount >> 3;
	rowSize = (bytesPerRow + 3) & ~3;
}
/*
	new: (x: int, y: int)
	{
		super(x, y, 0)
		header.biBitCount = 24
		bytesPerRow := x * header.biBitCount >> 3
		rowSize = (bytesPerRow + 3) & ~3;
		header.biSizeImage = rowSize * y
		data = unmanaged new [header.biSizeImage] byte
	}

	setCell: (x: int, y: int, index: int) 
	{
		if ((index & 0xff000000) != 0)
			return
		if (x < 0 || x >= cols)
			return
		if (y < 0 || y >= rows)
			return

		cell := y * rowSize + x * 3
		data[cell] = byte(index)
		data[cell + 1] = byte(index >> 8)
		data[cell + 2] = byte(index >> 16)
    }
*/
int BitMap24::getCell(int x, int y) {
	if (x < 0 ||
		x >= cols ||
		y < 0 ||
		y >= rows)
		return -1;
	int cell = y * rowSize + x * 3;
	return data[cell] + (data[cell + 1] << 8) + (data[cell + 2] << 16);
}
/*
	getColor: (x: int, y: int) int
	{return getCell(x, y)
	}

}
*/
BitMap* loadBitMap(const string& filename) {
	FILE* fp = fileSystem::openBinaryFile(filename);
	if (fp == null)
		fatalMessage("Couldn't open file: " + filename);
	fseek(fp, 0, SEEK_END);
	int rlen = ftell(fp) - 2;				// ignore the magic number
	fseek(fp, 0, SEEK_SET);

	short magic;

	if (fread(&magic, sizeof magic, 1, fp) != 1) {
		fprintf(stderr, "Could not read the magic number of file %s\n", filename.c_str());
		exit(1);
	}
	byte* image = new byte[rlen];
	int actual = fread(image, 1, rlen, fp);
	fclose(fp);
	if (actual != rlen) {
		fprintf(stderr, "Could not read contents of file %s\n", filename.c_str());
		exit(1);
	}
	if (magic != BITMAP_MAGIC_NUMBER){
		fprintf(stderr, "Bad magic number in map file: %s\n", filename.c_str());
		exit(1);
	}
	BITMAPINFO* header = (BITMAPINFO*)&image[12];
	if (header->bmiHeader.biBitCount == 4)
		return new BitMap4(image, filename);
	else if (header->bmiHeader.biBitCount == 8)
		return new BitMap8(image, filename);
	else if (header->bmiHeader.biBitCount == 24)
		return new BitMap24(image, filename);
	else
		return null;
}
/*
BitMap: public type {
	filename:	string
	rows, cols:	int
	rowSize:	int
	table:		pointer [?] int
	data:		pointer [?] byte
	image:		pointer [?] byte
	header:		pointer windows.BITMAPINFO
	dBitmap:	display.Bitmap

	new: (x: int, y: int, clrs: int)
	{
		rows = y
		cols = x
		header = unmanaged.alloc(sizeof windows.BITMAPINFO + clrs * 4)
		header.biSize = sizeof windows.BITMAPINFO
		header.biWidth = x
		header.biHeight = y
		header.biPlanes = 1
		header.biClrUsed = clrs
		header.biClrImportant = clrs
		table = pointer [?] int(pointer [?] byte(header) + sizeof windows.BITMAPINFO)
	}
	*/
BitMap::BitMap(byte *image) {
	this->image = image;
	header = (BITMAPINFO*)(&image[12]);
	cols = header->bmiHeader.biWidth;
	rows = header->bmiHeader.biHeight;
	table = (int*)(&image[12 + header->bmiHeader.biSize]);
	DWORD clrs = header->bmiHeader.biClrUsed;
	if (header->bmiHeader.biClrUsed == 0 &&
		header->bmiHeader.biBitCount < 16)
		clrs = 1 << header->bmiHeader.biBitCount;
	data = (byte*)(table + clrs);
	int bytesPerRow = cols * header->bmiHeader.biBitCount >> 3;
	rowSize = (bytesPerRow + 4) & ~3;
	dBitmap = null;
}

BitMap::BitMap() {
	table = null;
	data = null;
	image = null;
	header = null;
	dBitmap = null;
	rows = 0;
	cols = 0;
	rowSize = 0;
}
	/*
	saveAs: (filename: string) boolean
	{
		this.filename = filename
		fp := fileSystem.createBinaryFile(filename)
		if (fp == null)
			return false
		magic := BITMAP_MAGIC_NUMBER
		fp.write(pointer [?] byte(&magic), 2)
		dword := header.biSizeImage + header.biSize + header.biClrUsed * 4
		fp.write(pointer [?] byte(&dword), 4)
		dword = 0
		fp.write(pointer [?] byte(&dword), 4)
		dword = header.biSize + header.biClrUsed * 4
		fp.write(pointer [?] byte(&dword), 4)
		fp.write(pointer [?] byte(header), header.biSize)
		if (header.biClrUsed != 0)
			fp.write(pointer [?] byte(table), header.biClrUsed * 4)
		fp.write(data, header.biSizeImage)
		fp.close()
		return true
	}
*/
display::Bitmap* BitMap::getDisplayBitmap(display::Device* b) {
	if (dBitmap == null)
		dBitmap = new display::Bitmap(b, (BITMAPINFO*)header, data);
	return dBitmap;
}

BitMap* BitMap::dupHeader() {
	return null;
}
/*
	setCell: (x: int, y: int, index: int) 
	{
    }

	getColor: (x: int, y: int) int
	{
		return 0
	}
*/
bool BitMap::setColorPalette(int index, int color) {
	return false;
}

int BitMap::getColorPalette(int index) {
	return 0;
}

ScaledParcMap::ScaledParcMap(engine::ScaledBitMap *b) {
	bitMap = b;
}

xpoint ScaledParcMap::hexScale(float kilometersPerHex, int rotation) {
	deltax = 10000.0f;
	deltay = 10000.0f;
	xpoint p;

	p.x = xcoord(100);
	p.y = xcoord(100);
	return p;
}

ComboBitMap::ComboBitMap(xml::Document *doc, engine::HexMap *xmp, const string &filename, xml::Element *e) {
	this->xmp = xmp;
	componentMaps = null;
	ComboBitMapItem* lastCbmi;
	for (xml::Element* c = e->child; c != null; c = c->sibling) {
		BitMap* b = null;
		xml::Element* ec = doc->getValue(*c->getValue("src"));
		if (ec == null)
			fatalMessage("Unknown map: " + *c->getValue("src"));
		if (ec->getValue("laea") != null){
			b = new engine::LambertBitMap(xmp, *ec->getValue("src"), ec);
		} else if (ec->getValue("conical") != null){
//			b = new engine.ConicalBitMap(xmp, ec..src, ec)
		} else if (ec->getValue("scaled") != null){
			b = new engine::ScaledBitMap(xmp, *ec->getValue("src"), ec);
		} else
			fatalMessage("Cannot combine " + *ec->getValue("src"));
		ComboBitMapItem* cbmi = new ComboBitMapItem;
		cbmi->next = null;
		cbmi->realMap = b;
		cbmi->ignoreIndex = atoi(c->getValue("ignore")->c_str());
		string* clear = c->getValue("clear");
		if (clear)
			cbmi->clearIndex = atoi(clear->c_str());
		else
			cbmi->clearIndex = 0;
		string* colorBase = c->getValue("colorBase");
		if (colorBase)
			cbmi->colorBase = atoi(colorBase->c_str());
		else
			cbmi->colorBase = 0;
		if (componentMaps == null)
			componentMaps = cbmi;
		else
			lastCbmi->next = cbmi;
		lastCbmi = cbmi;
	}
}

int ComboBitMap::getCell(int x, int y) {
	for (ComboBitMapItem* cbmi = componentMaps; cbmi != null; cbmi = cbmi->next){
		int i = cbmi->realMap->getCell(x, y);
		if (i < cbmi->ignoreIndex && i != cbmi->clearIndex)
			return i + cbmi->colorBase;
		i = i;
	}
	return 63;
}

ScaledBitMap::ScaledBitMap() {
	realMap = null;
	xmp = null;
	baseY = 0;
	deltaY = 0;
	baseLat = 0;
	deltaLat = 0;
	baseLong = 0;
	deltaLong = 0;
}

ScaledBitMap::ScaledBitMap(engine::HexMap *xmp, const string &filename,
						   int x1, int y1, int x2, int y2,
						   const string &loc1, const string &loc2) {
	this->xmp = xmp;
	this->filename = filename;
	float lat1, long1;
	float lat2, long2;
	if (xmp->places.mapLocationToCoordinates(loc1, &lat1, &long1) &&
		xmp->places.mapLocationToCoordinates(loc2, &lat2, &long2)){
		realMap = loadBitMap(filename);
		init(x1, y1, lat1, long1, x2, y2, lat2, long2);
	}
}

ScaledBitMap::ScaledBitMap(engine::HexMap *xmp, const string &filename, xml::Element *e) {
	this->xmp = xmp;
	this->filename = filename;
	realMap = loadBitMap(filename);
	int x1 = atoi(e->getValue("x1")->c_str());
	int y1 = atoi(e->getValue("y1")->c_str());
	int x2 = atoi(e->getValue("x2")->c_str());
	int y2 = atoi(e->getValue("y2")->c_str());
	string* loc1 = e->getValue("loc1");
	float lat1, long1;
	if (loc1 != null) {
		if (!xmp->places.mapLocationToCoordinates(*loc1, &lat1, &long1)) {
			warningMessage("Could not find location " + *loc1);
			lat1 = 0;
			long1 = 0;
		}
	} else {
		lat1 = parseDegrees(*e->getValue("lat1"));
		long1 = parseDegrees(*e->getValue("long1"));
	}
	string* loc2 = e->getValue("loc2");
	float lat2, long2;
	if (loc2 != null) {
		if (!xmp->places.mapLocationToCoordinates(*loc2, &lat2, &long2)) {
			warningMessage("Could not find location " + *loc2);
			lat2 = 0;
			long2 = 0;
		}
	} else {
		lat2 = parseDegrees(*e->getValue("lat2"));
		long2 = parseDegrees(*e->getValue("long2"));
	}
	data.filename = filename;
	if (loc1)
		data.loc1.name = *loc1;
	data.loc1.x = x1;
	data.loc1.y = y1;
	if (loc2)
		data.loc2.name = *loc2;
	data.loc2.x = x2;
	data.loc2.y = y2;
	init(x1, y1, lat1, long1, x2, y2, lat2, long2);
}

void ScaledBitMap::init(int x1, int y1, float lat1, float long1, int x2, int y2, float lat2, float long2) {
	double baseLatitude = xmp->header.latitude + xmp->header.height / 2;
//		compression := sqrt(90*90 - baseLatitude * baseLatitude) / 90
//		baseLongitude := xmp.longitude - xmp.width / (2 * compression)
//		comp1 := sqrt(90*90 - lat1 * lat1) / 90
	double ny1 = xmp->getRows() * (baseLatitude - lat1) / xmp->header.height;
//		nx1 := xmp.cols / 2 + 
//				xmp.cols * (long1 - xmp.longitude) * comp1 / 
//							xmp.width
//		comp2 := sqrt(90*90 - lat2 * lat2) / 90
	double ny2 = xmp->getRows() * (baseLatitude - lat2) / xmp->header.height;
//		nx2 := xmp.cols / 2 + 
//				xmp.cols * (long2 - xmp.longitude) * comp2 / 
//							xmp.width
//		deltaX = (x2 - x1) / float(nx2 - nx1)
	deltaY = (y2 - y1) / (ny2 - ny1);
//		baseX = x2 - float(deltaX * nx2)
	baseY = y2 - deltaY * ny2;
	deltaLat = (lat2 - lat1) / (y2 - y1);
	baseLat = lat2 - y2 * deltaLat;
	deltaLong = (long2 - long1) / (x2 - x1);
	baseLong = long2 - x2 * deltaLong;
}

bool ScaledBitMap::freshen(const string &filename, 
						   int x1, int y1, int x2, int y2, 
						   const string &loc1, const string &loc2) {
	float lat1, long1;
	float lat2, long2;
	if (!xmp->places.mapLocationToCoordinates(loc1, &lat1, &long1) ||
		!xmp->places.mapLocationToCoordinates(loc2, &lat2, &long2))
		return false;
	if (this->filename != filename){
		BitMap* r = loadBitMap(filename);
		if (r == null)
			return false;
		realMap = r;
	}
	data.filename = filename;
	data.loc1.name = loc1;
	data.loc1.x = x1;
	data.loc1.y = y1;
	data.loc2.name = loc2;
	data.loc2.x = x2;
	data.loc2.y = y2;
	init(x1, y1, lat1, long1, x2, y2, lat2, long2);
	return true;
}

int ScaledBitMap::getCell(int x, int y) {
	int y0 = int(baseY + deltaY * y / 10000.0);
	double lat = baseLat + deltaLat * y0;
	double compression = sqrt(90*90 - lat * lat) / 90;
	double long0 = xmp->header.longitude + ((x / 10000.0f) - xmp->getColumns() / 2) * xmp->header.width / 
													(xmp->getColumns() * compression);
	int x0 = int((long0 - baseLong) / deltaLong);
	return realMap->getCell(x0, realMap->rows - y0);
}
/*
ConicalBitMap: public type inherits ScaledBitMap {
	realMap:		BitMap
	xmp:			HexMap
//	baseLat:		float
	deltaLat:		float
	xmpBaseLat:		float
	xmpDeltaLat:	float
	baseLong:		float
	poleY:			double
//	cosTheta1:		double
//	sinTheta1:		double
	scale:			float
	halfRows:		int
//	stretchX:		float
	lambda0Col:		int

	new: (xmp: HexMap, filename: string, e: xml.Element)
	{
		this.xmp = xmp
		latC := parseDegrees(e..lat)
//		longC := parseDegrees(e..long)
		height := float(e..height)
		realMap = loadBitMap(filename)
		halfRows = realMap.rows / 2
		baseLat = latC - height / 2
		deltaLat = height / realMap.rows
		lambda0Col = realMap.cols / 2
		s := e..lambda0Col
		if (s != null)
			lambda0Col = int(s)
		baseLong = parseDegrees(e..long)

		xmpDeltaLat = -xmp.height / (xmp.rows * 10000.0f)
		xmpBaseLat = xmp.latitude + xmp.height / 2

		scale = 90 / deltaLat
		poleY = halfRows - (90 - latC) / deltaLat
	}

    getCell: (x: int, y: int) int
	{
		lat := xmpBaseLat + y * xmpDeltaLat
		compression := sqrt(90*90 - lat * lat) / 90
		deltaX := ((x / 10000.0f) - xmp.cols / 2)
		long0 := xmp.longitude + xmp.width * deltaX / (compression * xmp.cols)
//		theta := 3.14159 * lat / 180									// radians
		lambda := 3.14159 * (long0 - baseLong) / 180		// in radians
		sinLambda := sin(lambda)
		cosLambda := cos(lambda)
		y0 := int(poleY + (90 - lat) * cosLambda / deltaLat)
		x0 := int(lambda0Col + scale * sinLambda)
		c := realMap.getCell(x0, realMap.rows - y0)
		return paletteMap[c]
    }

}

paletteMap: [] byte = [
	0,				// 0
	0,
	0,
	0,
	0,				// 4
	0,
	0,
	0,
	0,				// 8
	0,
	0,
	0,
	0,				// 12
	0,
	0,
	0,
	0,				// 16
	0,
	0,
	0,
	0,				// 20
	0,
	0,
	0,
	0,				// 24
	0,
	0,
	0,
	0,				// 28
	0,
	6,
	0,
	0,				// 32
	6,
	0,
	0,
	0,				// 36
	0,
	0,
	0,
	0,
	0,				// 40
	0,
	0,
	0,
	0,				// 44
	0,
	0,
	0,
	0,				// 48
	0,
	0,
	0,
	0,				// 52
	0,
	0,
	0,
	0,				// 56
	0,
	0,
	0,
	0,				// 60
	0,
	0,
	0,
]
*/
LambertBitMap::LambertBitMap(HexMap* xmp, const string& filename, xml::Element* e) {
	this->xmp = xmp;
	double latC = parseDegrees(*e->getValue("lat"));
	double height = atof(e->getValue("height")->c_str());
	string* sS = e->getValue("stretchX");
	if (sS != null)
		stretchX = atof(sS->c_str());
	else
		stretchX = 1;
	realMap = loadBitMap(filename);
	halfRows = realMap->rows / 2.0f;
	sS = e->getValue("theta1Row");
	if (sS != null)
		halfRows = atof(sS->c_str());
	baseLat = latC - height / 2;
	deltaLat = height / realMap->rows;
	string* th = e->getValue("theta1");
	if (*th == "90"){
		cosTheta1 = 0;
		sinTheta1 = 1;
	} else {
		double theta1 = 3.14159f * atof(e->getValue("theta1")->c_str()) / 180;
		cosTheta1 = cos(theta1);
		sinTheta1 = sin(theta1);
	}
	lambda0Col = realMap->cols / 2.0f;
	string* s = e->getValue("lambda0Col");
	if (s != null)
		lambda0Col = atof(s->c_str());
	baseLong = parseDegrees(*e->getValue("long"));

	xmpDeltaLat = -xmp->header.height / (xmp->getRows() * 10000.0f);
	xmpBaseLat = xmp->header.latitude + xmp->header.height / 2;

	scale = 90 / deltaLat;

	double theta = 3.14159 * latC / 180;
	double sinTheta = sin(theta);
	double cosTheta = cos(theta);
	double kprime = scale * sqrt(2 / (1 + sinTheta1 * sinTheta + cosTheta1 * cosTheta));
	poleY = halfRows - kprime * (cosTheta1 * sinTheta - sinTheta1 * cosTheta);
}

int LambertBitMap::getCell(int x, int y) {
	double lat = xmpBaseLat + y * xmpDeltaLat;
	double compression = sqrt(90*90 - lat * lat) / 90;
	double deltaX = ((x / 10000.0f) - xmp->getColumns() / 2);
	double long0 = xmp->header.longitude + xmp->header.width * deltaX / (compression * xmp->getColumns());
	double theta = 3.14159 * lat / 180;									// radians
	double lambda = 3.14159 * (long0 - baseLong) / 180;		// in radians
	double sinTheta = sin(theta);
	double cosTheta = cos(theta);
	double sinLambda = sin(lambda);
	double cosLambda = cos(lambda);
	double cosThetaCosLambda = cosTheta * cosLambda;
	double kprime = scale * sqrt(2 / (1 + sinTheta * sinTheta1 + cosTheta1 * cosThetaCosLambda));
	int y0 = int(poleY + kprime * (cosTheta1 * sinTheta - sinTheta1 * cosThetaCosLambda));
	int x0 = int(lambda0Col + stretchX * kprime * cosTheta * sinLambda);
	return realMap->getCell(x0, y0);
}

ParcMap::ParcMap() {
	bitMap = null;
}

ParcMap::ParcMap(const string &filename, float latitude, float longitude, float width, float height, bool isElliptical) {
	this->bitMap = loadBitMap(filename);
	this->latitude = latitude;
	this->longitude = longitude;
	this->width = width;
	this->height = height;
	this->isElliptical = isElliptical;
	bitMap->setColorPalette(PARC_XRIVER, bitMap->getColorPalette(PARC_RIVER));
	bitMap->setColorPalette(PARC_XCOAST, bitMap->getColorPalette(PARC_COAST));
	bitMap->setColorPalette(PARC_XBORDER, bitMap->getColorPalette(PARC_BORDER));
}

xpoint ParcMap::hexScale(float kilometersPerHex, int rotation) {
	double hkm = height * (69 * 1.609);
	double chkLatitude = latitude - height / 2;
	if (chkLatitude < 0 && chkLatitude + height > 0)
		chkLatitude = 0;
	double wkm = width * (69 * 1.609) * cos((chkLatitude) * 3.14159 / 180);

	if ((rotation & 1) == 1){
		double x = hkm;
		hkm = wkm;
		wkm = x;
	}
	int hrows = int(hkm / kilometersPerHex);
	int hcols = int(wkm * sqrt3 / (1.5 * kilometersPerHex));

	deltax = bitMap->cols / float(hcols);
	deltay = -bitMap->rows / float(hrows);
	originy = bitMap->rows - 1;
	xpoint p;

	p.x = xcoord(hcols);
	p.y = xcoord(hrows);
	return p;
}
/*
	pixelToHex: (x: int, y: int) xpoint
	{
		res: xpoint

		scale1 := deltax// * 1.5f / sqrt3
		ry := 2 * (originy - y) / -deltay
		halfy := int(ry)
		hy := halfy / 2
		dy := ry - halfy
		ixbase := dy / 2
		if ((halfy & 1) == 0){
			intercepts[0] = (0.5f - ixbase) / 1.5f
			intercepts[1] = ixbase / 1.5f
			rx := x / scale1
			hx := int(rx)
			dx := rx - hx
			ix := intercepts[hx & 1]
			if (dx > ix){
				ix = 1 + intercepts[1 - (hx & 1)]
				res.x = xcoord(hx)
				if ((hx & 1) == 1){
					res.y = xcoord(hy - 1)
				} else {
					res.y = xcoord(hy)
				}
			} else {
				res.x = xcoord(hx - 1)
				if ((hx & 1) == 0)
					res.y = xcoord(hy - 1)
				else
					res.y = xcoord(hy)
			}
		} else {
			intercepts[1] = (0.5f - ixbase) / 1.5f;
			intercepts[0] = ixbase / 1.5f;
			rx := x / scale1
			hx := int(rx)
			dx := rx - hx
			ix := intercepts[hx & 1]
			if (dx > ix)
				res.x = xcoord(hx)
			else
				res.x = xcoord(hx - 1)
			res.y = xcoord(hy)
		}
		return res
	}

}
*/
RotatedBitMap::RotatedBitMap(BitMap *b, int rotation) {
	this->bitMap = b;
	this->rotation = rotation;
	switch (rotation){
	case	0:
	case	2:
		rows = b->rows;
		cols = b->cols;
		break
;
	case	-1:
	case	1:
		rows = b->cols;
		cols = b->rows;
		break;
	}
}
/*
	setCell: (x: int, y: int, index: int) 
	{
		switch (rotation){
		case	0:
			bitMap.setCell(x, y, index)
			break

		case	1:
			bitMap.setCell(rows - (y + 1), x, index)
			break

		case	-1:
			bitMap.setCell(y, cols - (x + 1), index)
			break

		case	2:
			bitMap.setCell(cols - (x + 1), rows - (y + 1), index)
		}
    }
*/
int RotatedBitMap::getCell(int x, int y) {
	switch (rotation){
	case	0:
		return bitMap->getCell(x, y);
		break;

	case	1:
		return bitMap->getCell(rows - (y + 1), x);
		break;

	case	-1:
		return bitMap->getCell(y, cols - (x + 1));
		break;

	case	2:
		return bitMap->getCell(cols - (x + 1), rows - (y + 1));
	}
	return -1;
}

bool RotatedBitMap::setColorPalette(int index, int color) {
	return bitMap->setColorPalette(index, color);
}

int RotatedBitMap::getColorPalette(int index) {
	return bitMap->getColorPalette(index);
}

}  // namespace engine
