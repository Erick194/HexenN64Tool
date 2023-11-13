// ===================================================================================================================
// Hexen N64 Tool
// ===================================================================================================================

#include <cstring>
#include <stdio.h>
#include <vector>
#include <cstdarg>
#include <string>
#include <conio.h>
#include <windows.h>
#include <png.h>
#include <zlib.h>
#include <algorithm>
#include <md5.h>

#include "HexenLZHuff.h"
#include "Sound.h"

typedef unsigned char byte;
#define _PAD4(x)	x += (4 - ((uint32_t) x & 3)) & 3

bool getFileMD5Hash(void* data, uint64_t numBytes, uint64_t& hashWord1, uint64_t& hashWord2) {

	if ((data == nullptr) || (numBytes == 0))
		return false;

	// Hash the data and turn the hash into 2 64-bit words
	MD5 md5Hasher;
	md5Hasher.reset();
	md5Hasher.add(data, numBytes);

	uint8_t md5[16] = {};
	md5Hasher.getHash(md5);

	hashWord1 = (
		((uint64_t)md5[0] << 56) | ((uint64_t)md5[1] << 48) | ((uint64_t)md5[2] << 40) | ((uint64_t)md5[3] << 32) |
		((uint64_t)md5[4] << 24) | ((uint64_t)md5[5] << 16) | ((uint64_t)md5[6] << 8) | ((uint64_t)md5[7] << 0)
		);

	hashWord2 = (
		((uint64_t)md5[8] << 56) | ((uint64_t)md5[9] << 48) | ((uint64_t)md5[10] << 40) | ((uint64_t)md5[11] << 32) |
		((uint64_t)md5[12] << 24) | ((uint64_t)md5[13] << 16) | ((uint64_t)md5[14] << 8) | ((uint64_t)md5[15] << 0)
		);

	return true;
}

bool checkFileMD5Hash(void* data, uint64_t numBytes, uint64_t checkHashWord1, uint64_t checkHashWord2) {
	uint64_t actualHashWord1 = {};
	uint64_t actualHashWord2 = {};

	if (getFileMD5Hash(data, numBytes, actualHashWord1, actualHashWord2)) {
		//printf("actualHashWord1 %I64X\n", actualHashWord1);
		//printf("actualHashWord2 %I64X\n", actualHashWord2);
		return ((actualHashWord1 == checkHashWord1) && (actualHashWord2 == checkHashWord2));
	}
	else {
		return false;
	}
}

HANDLE hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
COORD getcoord;

COORD GetConsoleCursorPosition(HANDLE hConsoleOutput)
{
	CONSOLE_SCREEN_BUFFER_INFO cbsi;
	if (GetConsoleScreenBufferInfo(hConsoleOutput, &cbsi))
	{
		return cbsi.dwCursorPosition;
	}
	else
	{
		// The function failed. Call GetLastError() for details.
		COORD invalid = { 0, 0 };
		return invalid;
	}
}

void setcolor(int color)
{
	SetConsoleTextAttribute(hConsoleOutput, color);
}

#define CLAMP(value, min, max) (((value) >(max)) ? (max) : (((value) <(min)) ? (min) : (value)))

void PrintfPorcentaje(int count, int Prc, bool draw, int ypos, const char* s, ...) {
	COORD coord;

	char msg[1024];
	va_list v;

	va_start(v, s);
	vsprintf(msg, s, v);
	va_end(v);

	coord.X = 0; coord.Y = ypos;
	SetConsoleCursorPosition(hConsoleOutput, coord);

	float prc = (float)CLAMP(((float)((count))) / (Prc), 0.0, 1.0);
	if (draw)
	{
		setcolor(0x07);
		printf("%s", msg);
		setcolor(0x0A);
		printf("(%%%.2f)        \n", prc * 100);
	}
	setcolor(0x07);
}

void ShowInfo(void)
{
	setcolor(0x07);
	printf("\n     ####################");
	printf("(ERICK194)");
	printf("###################\n");
	printf("     #                 HEXEN 64 TOOL                 #\n");
	printf("     #         CREATED BY ERICK VASQUEZ GARCIA       #\n");
	printf("     #                VERSION 1.0 (2023)             #\n");
	printf("     #                  Hexen.z64                    #\n");
	printf("     #         (USA/EUP/JAP/GER/FRA) VERSION         #\n");
	printf("     #                                               #\n");
	printf("     # USE:                                          #\n");
	printf("     # HexenN64Tool.exe -getfiles     \"RomImage.z64\" #\n");
	printf("     # HexenN64Tool.exe -encode       FileIn FileOut #\n");
	printf("     # HexenN64Tool.exe -decode       FileIn FileOut #\n");
	printf("     # HexenN64Tool.exe -encWad       FileIn FileOut #\n");
	printf("     # HexenN64Tool.exe -decWadNormal FileIn FileOut #\n");
	printf("     # HexenN64Tool.exe -decWadFormat FileIn FileOut #\n");
	printf("     #################################################\n");
	printf("\n");
}

void Error(const char* s, ...) {
	va_list args;
	setcolor(0x04);
	va_start(args, s);
	vfprintf(stderr, s, args);
	fprintf(stderr, "\n");
	va_end(args);
	setcolor(0x07);
	exit(EXIT_FAILURE);
}

enum RomRegion
{
	REGION_NUL = -1,
	REGION_USA,
	REGION_EUP,
	REGION_JAP,
	REGION_GER,
	REGION_FRA,
	REGION_MAX
};

byte header[65];
RomRegion region = REGION_NUL;

int ReadHeader(FILE* f, int offset)
{
	byte a;

	fseek(f, offset, SEEK_SET);

	for (int i = 0; i < 64; i++)
	{
		fread(&a, sizeof(byte), 1, f);
		header[i] = a;
	}

	if (header[0x3E] == 0x45) // Region USA
	{
		region = REGION_USA;
		printf("Region USA\n");
	}
	else if (header[0x3E] == 0x50) // Region EUP
	{
		region = REGION_EUP;
		printf("Region EUP\n");
	}
	else if (header[0x3E] == 0x4A) // Region JAP
	{
		region = REGION_JAP;
		printf("Region JAP\n");
	}
	else if (header[0x3E] == 0x44) // Region GER
	{
		region = REGION_GER;
		printf("Region GER\n");
	}
	else if (header[0x3E] == 0x46) // Region FRA
	{
		region = REGION_FRA;
		printf("Region FRA\n");
	}
	else
	{
		Error("Error Region %d\n", region);
		exit(0);
	}

	return 0;
}

struct fileList {
	int wadOffset, wadsize; // HEXEN64.WAD
	int ptrOffset, ptrsize; // HEXEN64.PTR
	int wbkOffset, wbksize; // HEXEN64.WBK
	int datOffset, datsize; // HEXEN64.DAT
};

fileList romFiles[REGION_MAX] = {
	{0x173D70, 0x63F480, 0x38270, 0x7B00, 0x3FD70, 0x134000, 0x7B31F0, -1}, // Region USA
	{0x174AD0, 0x63B630, 0x38FD0, 0x7B00, 0x40AD0, 0x134000, 0x7B0100, -1}, // Region EUP
	{0x184CC0, 0x642568, 0x38C10, 0x7B00, 0x40710, 0x1445B0, 0x7C7230, -1}, // Region JAP
	{0x172C50, 0x63E770, 0x38FD0, 0x7B00, 0x40AD0, 0x132180, 0x7B13C0, -1}, // Region GER
	{0x176040, 0x63DD58, 0x38FD0, 0x7B00, 0x40AD0, 0x135570, 0x7B3DA0, -1}  // Region FRA
};

byte name1[6] = { 'H','E','X','E','N' };

int CheckName(void)
{
	int i;
	int rtn;

	rtn = 1;
	for (i = 0; i < 5; i++)
	{
		if (header[0x20 + i] != name1[i])
		{
			rtn = 0;
			break;
		}
	}

	return rtn;
}

FILE* wadin;
FILE* wadout;

typedef struct
{
	int  filepos; // also texture_t * for comp lumps
	int  size;
	char name[8 + 1];
} lumpinfo_t;

typedef struct
{
	char identification[4]; // should be IWAD
	int  numlumps;
	int  infotableofs;
} wadinfo_t;

wadinfo_t wadfile;

lumpinfo_t* lumpinfo; // points directly to rom image
int         numlumps;

int SwapInt(int value)
{
	return ((value & 0x000000ff) << 24) | ((value & 0xff000000) >> 24) | ((value & 0x00ff0000) >> 8) | ((value & 0x0000ff00) << 8);
}

unsigned short SwapUShort(unsigned short value)
{
	return unsigned short((value & 0x00ff) << 8) | ((value & 0xff00) >> 8);
}

short SwapShort(short value)
{
	return short((value & 0x00ff) << 8) | ((value & 0xff00) >> 8);
}

int offcnt = 12;

//==========================================================================
//
// W_InitFile
//
// Initialize the primary from a single file.
//
//==========================================================================

void W_InitFile(char* name, bool encode)
{
	int i;
	if ((wadin = fopen(name, "r+b")) == 0) {
		Error("No encuentra el archivo %s\n", name);
	}

	fread(&wadfile, sizeof(wadfile), 1, wadin);

	if (!encode)
	{
		wadfile.numlumps = SwapInt(wadfile.numlumps);
		wadfile.infotableofs = SwapInt(wadfile.infotableofs);
	}

	numlumps = wadfile.numlumps;
	lumpinfo = (lumpinfo_t*)malloc(numlumps * sizeof(lumpinfo_t));

	fseek(wadin, wadfile.infotableofs, SEEK_SET);

	for (i = 0; i < numlumps; i++)
	{
		//fread (&lumpinfo[i],sizeof(lumpinfo_t), 1 ,file);
		fread(&lumpinfo[i].filepos, sizeof(unsigned int), 1, wadin);
		fread(&lumpinfo[i].size, sizeof(unsigned int), 1, wadin);
		fread(&lumpinfo[i].name, sizeof(unsigned int) * 2, 1, wadin);
		lumpinfo[i].name[8] = 0;

		if (!encode) {
			lumpinfo[i].filepos = SwapInt(lumpinfo[i].filepos);
			lumpinfo[i].size = SwapInt(lumpinfo[i].size);
		}
	}
}

uint8_t *W_ReadLumpName(char* name) {
	uint8_t* input, *output;
	int olen;

	for (int i = 0; i < numlumps; i++) {
		if (!strcmp(lumpinfo[i].name, name)) {
			//printf("%s\n", name);

			input = (uint8_t*)malloc(lumpinfo[i].size);

			fseek(wadin, lumpinfo[i].filepos, SEEK_SET);
			fread(input, sizeof(uint8_t), lumpinfo[i].size, wadin);

			olen = SwapInt(*(int*)input);
			output = (unsigned char*)malloc(olen);
			Decode(input + 4, output, olen);

			free(input);

			return output;
		}
	}

	Error("El lump %s no existe\n", name);
	return nullptr;
}

void W_AddLumpEntry(char* name, int size) {

	numlumps++;

	// Asignar memoria para el nuevo arreglo dinámico
	lumpinfo_t* newlumpinfo = (lumpinfo_t*)malloc(numlumps * sizeof(lumpinfo_t));

	// Copiar los elementos anteriores al nuevo arreglo
	for (int i = 0; i < numlumps - 1; i++) {
		newlumpinfo[i] = lumpinfo[i];
	}

	//setnew filepos
	newlumpinfo[numlumps - 1].filepos = offcnt;
	newlumpinfo[numlumps - 1].size = size;
	strncpy(newlumpinfo[numlumps - 1].name, name, 8);

	// Liberar la memoria del antiguo arreglo y apuntar al nuevo
	free(lumpinfo);
	lumpinfo = newlumpinfo;
	offcnt += size;
}

int W_CheckNumForName(char* name)
{
	char name8[9];
	int v1, v2;
	lumpinfo_t* lump_p;

	// Make the name into two integers for easy compares
	strncpy(name8, name, 8);
	name8[8] = 0; // in case the name was a full 8 chars
	strupr(name8); // case insensitive
	v1 = *(int*)name8;
	v2 = *(int*)&name8[4];

	// Scan backwards so patch lump files take precedence
	lump_p = lumpinfo + numlumps;
	while (lump_p-- != lumpinfo)
	{
		if (*(int*)lump_p->name == v1 && *(int*)&lump_p->name[4] == v2)
		{
			return lump_p - lumpinfo;
		}
	}

	Error("W_CheckNumForName: %s not found!", name);
	return -1;
}

typedef struct
{
	unsigned short  width;
	unsigned short  height;
	short           xoffs;
	short           yoffs;
	unsigned short	cmpsize;
	unsigned short	wshift;
	unsigned short	hshift;
	short			pad;
} hexenGFXHeaderN64_t;

bool checkImage(unsigned char* input) {

	int wshift, hshift, shift, width, height;
	hexenGFXHeaderN64_t* header = (hexenGFXHeaderN64_t*)input;

	width = SwapUShort(header->width);
	height = SwapUShort(header->height);
	//printf("width %d\n", width);
	//printf("height %d\n", height);

	if (width == 0) {
		return false;
	}
	if (height == 0) {
		return false;
	}

	if (width & 1) {
		width++;
	}

	if (height & 1) {
		height++;
	}

	shift = 1;
	for (wshift = 0; wshift < 16; wshift++, shift <<= 1) {
		if (width <= shift) {
			//width = shift;
			//printf("shift %d\n", shift);
			break;
		}
	}

	shift = 1;
	for (hshift = 0; hshift < 16; hshift++, shift <<= 1) {
		if (height <= shift) {
			//height = shift;
			//printf("shift %d\n", shift);
			break;
		}
	}

	//printf("wshift %d\n", wshift);
	//printf("hshift %d\n", hshift);

	//printf("_wshift %d\n", SwapUShort(header->wshift));
	//printf("_hshift %d\n", SwapUShort(header->hshift));

	if ((SwapUShort(header->wshift) == wshift) && (SwapUShort(header->hshift) == hshift)) {
		return true;
	}

	//_getch();
	return false;
}


// **************************************************************
// Paletes
// **************************************************************
uint8_t* g_playPal = nullptr;
uint8_t* g_titlePal = nullptr;
uint8_t* g_creditPal = nullptr;
uint8_t* g_logoGTPal = nullptr;
uint8_t* g_logoIDPal = nullptr;
uint8_t* g_logoSCPal = nullptr;
uint8_t* g_logoRAVPal = nullptr;
uint8_t* g_legalsPal = nullptr;
int g_begMusic = -1;
int g_endMusic = -1;

// **************************************************************
// PNG Code
// **************************************************************
typedef byte* cache;
static cache writeData;
static unsigned int current = 0;

//**************************************************************
//**************************************************************
//  Png_WriteData
//
//  Work with data writing through memory
//**************************************************************
//**************************************************************

static void Png_WriteData(png_structp png_ptr, cache data, size_t length) {
	writeData = (byte*)realloc(writeData, current + length);
	memcpy(writeData + current, data, length);
	current += length;
}

//**************************************************************
//**************************************************************
//  Png_Create
//
//  Create a PNG image through memory
//**************************************************************
//**************************************************************

cache Png_Create(cache data, byte* paletteBuff, int* size, int width, int height, int offsetx = 0, int offsety = 0, int num_trans = -1)
{
	int i, j;
	cache image;
	cache out;
	cache* row_pointers;
	png_structp png_ptr;
	png_infop info_ptr;
	png_colorp palette;
	png_bytep trans;

	int bit_depth = 8;
	int palsize = 256;


	// setup png pointer
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (png_ptr == NULL) {
		Error("Png_Create: Failed getting png_ptr");
		return NULL;
	}

	// setup info pointer
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		Error("Png_Create: Failed getting info_ptr");
		return NULL;
	}

	// what does this do again?
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		Error("Png_Create: Failed on setjmp");
		return NULL;
	}

	// setup custom data writing procedure
	png_set_write_fn(png_ptr, NULL, Png_WriteData, NULL);

	// setup image
	png_set_IHDR(
		png_ptr,
		info_ptr,
		width,
		height,
		bit_depth,
		PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	// setup palette
	palette = (png_colorp)png_malloc(png_ptr, palsize * sizeof(png_color));

	for (int x = 0; x < 256; x++)
	{
		//printf("x %d\n", x);
		palette[x].red	= *paletteBuff++;
		palette[x].green = *paletteBuff++;
		palette[x].blue = *paletteBuff++;
	}

	png_set_PLTE(png_ptr, info_ptr, palette, palsize);

	if (num_trans != -1)
	{
		trans = (png_bytep)png_malloc(png_ptr, num_trans + 1);
		for (int tr = 0; tr < num_trans + 1; tr++)
		{
			if (tr == num_trans) { trans[tr] = 0; }
			else { trans[tr] = 255; }
		}
		png_set_tRNS(png_ptr, info_ptr, trans, num_trans + 1, NULL);
		png_free(png_ptr, trans);
	}

	// add png info to data
	png_write_info(png_ptr, info_ptr);

	if (offsetx != 0 || offsety != 0)
	{
		int offs[2];

		offs[0] = SwapInt(offsetx);
		offs[1] = SwapInt(offsety);

		png_write_chunk(png_ptr, (png_byte*)"grAb", (byte*)offs, 8);
	}

	// setup packing if needed
	png_set_packing(png_ptr);
	png_set_packswap(png_ptr);

	// copy data over
	byte inputdata;
	image = data;
	row_pointers = (cache*)malloc(sizeof(byte*) * height);
	for (i = 0; i < height; i++)
	{
		row_pointers[i] = (cache)malloc(width);
		for (j = 0; j < width; j++)
		{
			inputdata = *image;
			//if(inputdata == 0x7f){inputdata = (byte)num_trans;}
			row_pointers[i][j] = inputdata;
			image++;
		}
	}

	// cleanup
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	png_free(png_ptr, palette);
	png_free(png_ptr, row_pointers);
	palette = nullptr;
	row_pointers = nullptr;
	png_destroy_write_struct(&png_ptr, &info_ptr);

	// allocate output
	out = (cache)malloc(current);
	memcpy(out, writeData, current);
	*size = current;

	free(writeData);
	writeData = nullptr;
	current = 0;

	return out;
}

void CopyLump(int lump, bool format, bool encode)
{
	unsigned char* input = nullptr, *output = nullptr;
	unsigned char data;
	int i;
	int size = 0;
	int pow = 0;
	int ilen = 0;
	int olen = 0;
	uint8_t* curPal = nullptr;
	int num_trans = 255;
	bool n64PaltoRgb = false;
	bool songToMidi = false;

	fseek(wadin, lumpinfo[lump].filepos, SEEK_SET);

	// set new filepos
	lumpinfo[lump].filepos = offcnt;

	if (!strcmp(lumpinfo[lump].name, "TITLEBMP")) {
		curPal = g_titlePal;
		num_trans = -1;
	}
	else if (!strcmp(lumpinfo[lump].name, "CREDIT1") || !strcmp(lumpinfo[lump].name, "CREDIT2") || !strcmp(lumpinfo[lump].name, "CREDIT3")) {
		curPal = g_creditPal;
	}
	else if (!strcmp(lumpinfo[lump].name, "LOGOGT")) {
		curPal = g_logoGTPal;
	}
	else if (!strcmp(lumpinfo[lump].name, "LOGOID")) {
		curPal = g_logoIDPal;
	}
	else if (!strcmp(lumpinfo[lump].name, "LOGOSC")) {
		curPal = g_logoSCPal;
	}
	else if (!strcmp(lumpinfo[lump].name, "LOGORAV")) {
		curPal = g_logoRAVPal;
	}
	else if (!strcmp(lumpinfo[lump].name, "LEGALS")) {
		curPal = g_legalsPal;
	}
	else if (!strcmp(lumpinfo[lump].name, "FIREPAL")) {
		n64PaltoRgb = true;
	}
	else if ((lump >= g_begMusic) && ((lump <= g_endMusic))) {
		songToMidi = true;
	}
	else {
		curPal = g_playPal;
	}

	if (lumpinfo[lump].size != 0)
	{
		ilen = lumpinfo[lump].size;
		//printf("lumpinfo[%d].name %s\n", lump, lumpinfo[lump].name);

		input = (unsigned char*)malloc(ilen);
		fread(input, sizeof(unsigned char), ilen, wadin);

		if (encode) {
			output = (unsigned char*)malloc(ilen << 1);
			olen = Encode(input, output, ilen);
			olen = _PAD4(olen);
			fwrite(output, sizeof(byte), olen, wadout);
		}
		else {
			// [GEC] Corrige un byte en el archivo comprimido de lump "WRTHE3E7".
			// Este error ocurre únicamente en la versión japonesa, lo que provoca que el juego se bloquee al descomprimir el archivo de manera incorrecta.
			//
			// [GEC] Fix a byte in the compressed lump file "WRTHE3E7." 
			// This error occurs only in the Japanese version, causing the game to crash as the file is decompressed incorrectly.
			{
				if (!strcmp(lumpinfo[lump].name, "WRTHE3E7") && checkFileMD5Hash(input, ilen, 0xB6EE6C34465832B8, 0x7B1CCC789284DE66)) {
					input[1383] = 0x36;
				}
			}

			olen = SwapInt(*(int*)input);
			output = (unsigned char*)malloc(olen);
			Decode(input + 4, output, olen);
			int skipData = 0;
			if (format) {
				if (checkImage(output)) {
					hexenGFXHeaderN64_t* header = (hexenGFXHeaderN64_t*)output;
					header->width = SwapUShort(header->width);
					header->height = SwapUShort(header->height);
					header->xoffs = SwapShort(header->xoffs);
					header->yoffs = SwapShort(header->yoffs);
					int size = 0;
					//printf("lump -> %d\n", lump);
					cache pngout = Png_Create(output + sizeof(hexenGFXHeaderN64_t), curPal, &size, header->width, header->height, header->xoffs, header->yoffs, num_trans);
					fwrite(pngout, sizeof(byte), size, wadout);
					free(pngout);

					olen = size;
				}
				else if (n64PaltoRgb) {
					int size = 0;
					uint8_t* n64Pal = (uint8_t*)malloc(olen * 2);
					unsigned char* out = output;

					for (i = 0; i < olen / 2; i++) {
						short val = *(short*)out;
						out += 2;
						val = SwapShort(val);

						// Unpack and expand to 8bpp, then flip from BGR to RGB.
						byte b = (val & 0x003E) << 2;
						byte g = (val & 0x07C0) >> 3;
						byte r = (val & 0xF800) >> 8;
						n64Pal[size++] = r;
						n64Pal[size++] = g;
						n64Pal[size++] = b;
					}

					olen = size;
					fwrite(n64Pal, sizeof(byte), olen, wadout);
					free(n64Pal);
				}
				else if (songToMidi) {
					MusStartSong(output, nullptr);
					uint8_t* midiBuff = Song2Midi();
					olen = *(int*)midiBuff;
					fwrite(midiBuff + sizeof(int), sizeof(byte), olen, wadout);
				}
				else {
					fwrite(output + skipData, sizeof(byte), olen, wadout);
				}
			}
			else {
				fwrite(output + skipData, sizeof(byte), olen, wadout);
			}
		}
		size += olen;

		free(input);
		free(output);
	}
	else
	{
		for (i = 0; i < lumpinfo[lump].size; i++) {
			fread(&data, sizeof(unsigned char), 1, wadin);
			fputc(data, wadout);
			size++;
			olen++;
		}
	}

	lumpinfo[lump].size = size;
	offcnt += olen;
}

int main(int argc, char* argv[])
{
	int i;
	bool getDatas = false;
	bool encode = false;
	bool decode = false;
	bool encWad = false;
	bool decWad = false;
	bool decWadFormat = false;
	byte *tmpBuff = nullptr;
	byte *decBuff = nullptr;

	// Parse arguments
	char* fileName = nullptr;
	char* outName = nullptr;
	for (i = 1; i < argc; i++)
	{
		argv[i] = strlwr(argv[i]);
		if (argv[i][0] == '-')
		{
			if (!strcmp(argv[i], "-getfiles"))
			{
				getDatas = true;
				i++;
				fileName = argv[i];
				break;
			}
			else if (!strcmp(argv[i], "-encode"))
			{
				encode = true;
				i++;
				fileName = argv[i];
				i++;
				outName = argv[i];
				break;
			}
			else if (!strcmp(argv[i], "-decode"))
			{
				decode = true;
				i++;
				fileName = argv[i];
				i++;
				outName = argv[i];
				break;
			}
			else if (!strcmp(argv[i], "-encwad"))
			{
				encWad = true;
				i++;
				fileName = argv[i];
				i++;
				outName = argv[i];
				break;
			}
			else if (!strcmp(argv[i], "-decwadnormal"))
			{
				decWad = true;
				i++;
				fileName = argv[i];
				i++;
				outName = argv[i];
				break;
			}
			else if (!strcmp(argv[i], "-decwadformat"))
			{
				decWadFormat = true;
				i++;
				fileName = argv[i];
				i++;
				outName = argv[i];
				break;
			}
		}
	}

	ShowInfo();
	if (argc == 1) {
		exit(0);
	}

	if (!fileName) {
		Error("Input file name is missing\n");
	}

	getcoord.Y = 19;
	setcolor(0x07);
		 
	if (getDatas) {
		FILE* in, * out;
		in = fopen(fileName, "rb");

		if (!in) {
			Error("Cannot open %s\n", fileName);
		}

		ReadHeader(in, 0);

		//printf("CheckName :: %d\n", CheckName());
		if (CheckName() == 0) {
			Error("Not a Hexen 64 Rom\n");
		}

		// Extract the file HEXEN64.WAD
		printf("Extracting HEXEN64.WAD\n");
		fseek(in, romFiles[region].wadOffset, SEEK_SET);
		tmpBuff = (byte*)malloc(romFiles[region].wadsize);
		out = fopen("HEXEN64.WAD", "wb");
		fread(tmpBuff, sizeof(byte), romFiles[region].wadsize, in);
		fwrite(tmpBuff, sizeof(byte), romFiles[region].wadsize, out);
		fclose(out);
		free(tmpBuff);

		// Extract the file HEXEN64.PTR
		printf("Extracting HEXEN64.PTR\n");
		fseek(in, romFiles[region].ptrOffset, SEEK_SET);
		tmpBuff = (byte*)malloc(romFiles[region].ptrsize);
		out = fopen("HEXEN64.PTR", "wb");
		fread(tmpBuff, sizeof(byte), romFiles[region].ptrsize, in);
		fwrite(tmpBuff, sizeof(byte), romFiles[region].ptrsize, out);
		fclose(out);
		free(tmpBuff);

		// Extract the file HEXEN64.WBK
		printf("Extracting HEXEN64.WBK\n");
		fseek(in, romFiles[region].wbkOffset, SEEK_SET);
		tmpBuff = (byte*)malloc(romFiles[region].wbksize);
		out = fopen("HEXEN64.WBK", "wb");
		fread(tmpBuff, sizeof(byte), romFiles[region].wbksize, in);
		fwrite(tmpBuff, sizeof(byte), romFiles[region].wbksize, out);
		fclose(out);
		free(tmpBuff);

		// Extract the file HEXEN64.DAT
		printf("Extracting HEXEN64.DAT\n");
		if (romFiles[region].datsize == -1) {
			fseek(in, 0, SEEK_END);
			romFiles[region].datsize = ftell(in) - romFiles[region].datOffset;
		}
		fseek(in, romFiles[region].datOffset, SEEK_SET);
		tmpBuff = (byte*)malloc(romFiles[region].datsize);
		out = fopen("HEXEN64.DAT", "wb");
		fread(tmpBuff, sizeof(byte), romFiles[region].datsize, in);

		int olen = SwapInt(*(int*)tmpBuff);
		decBuff = (unsigned char*)malloc(olen);
		Decode(tmpBuff + 4, decBuff, olen);

		fwrite(decBuff, sizeof(byte), olen, out);
		fclose(out);
		free(tmpBuff);
		free(decBuff);

		fclose(in);
	}

	if (encode) {
		if (!outName) {
			Error("Output file name is missing\n");
		}

		FILE* in, * out;
		byte* input, * output;
		in = fopen(fileName, "rb");

		if (!in) {
			Error("Cannot open %s\n", fileName);
		}

		printf("Compressing %s...\n", fileName);

		fseek(in, 0, SEEK_END);
		int ilen = ftell(in);
		fseek(in, 0, SEEK_SET);

		input = (unsigned char*)malloc(ilen);
		output = (unsigned char*)malloc(ilen);

		fread(input, sizeof(byte), ilen, in);

		out = fopen(outName, "wb");

		int olen = Encode(input, output, ilen);
		olen = _PAD4(olen);

		fwrite(output, sizeof(byte), olen, out);

		free(input);
		free(output);
		fclose(out);
		fclose(in);
	}

	if (decode) {
		if (!outName) {
			Error("Output file name is missing\n");
		}

		FILE* in, * out;
		byte* input, * output;
		in = fopen(fileName, "rb");

		if (!in) {
			Error("Cannot open %s\n", fileName);
		}

		printf("Decompressing %s...\n", fileName);

		fseek(in, 0, SEEK_END);
		int ilen = ftell(in);
		fseek(in, 0, SEEK_SET);

		input = (unsigned char*)malloc(ilen);
		output = (unsigned char*)malloc(ilen);

		fread(input, sizeof(byte), ilen, in);

		out = fopen(outName, "wb");

		int olen = SwapInt(*(int*)input);
		output = (unsigned char*)malloc(olen);
		Decode(input + 4, output, olen);

		fwrite(output, sizeof(byte), olen, out);

		free(input);
		free(output);
		fclose(out);
		fclose(in);
	}

	if (encWad) {
		if (!outName) {
			Error("Output file name is missing\n");
		}

		W_InitFile(fileName, true);

		wadout = fopen(outName, "wb");
		fwrite(&wadfile, sizeof(wadfile), 1, wadout);

		for (i = 0; i < numlumps; i++)
		{
			PrintfPorcentaje(i, numlumps - 1, true, getcoord.Y, "Encoding %s...\t", argv[2]);
			CopyLump(i, decWadFormat, true);
		}

		setcolor(0x07);

		for (i = 0; i < numlumps; i++)
		{
			lumpinfo[i].filepos = SwapInt(lumpinfo[i].filepos);
			lumpinfo[i].size = SwapInt(lumpinfo[i].size);

			fwrite(&lumpinfo[i].filepos, sizeof(unsigned int), 1, wadout);
			fwrite(&lumpinfo[i].size, sizeof(unsigned int), 1, wadout);
			fwrite(&lumpinfo[i].name, sizeof(unsigned int) * 2, 1, wadout);
		}

		fseek(wadout, 0, SEEK_SET);

		wadfile.numlumps = SwapInt(numlumps);
		wadfile.infotableofs = SwapInt(offcnt);
		fwrite(&wadfile, sizeof(wadfile), 1, wadout);

		free(lumpinfo);
		fclose(wadout);
		fclose(wadin);
	}

	if (decWad || decWadFormat) {

		if (!outName) {
			Error("Output file name is missing\n");
		}

		if (decWadFormat) {
			Sound_Setup();
		}

		W_InitFile(fileName, false);

		wadout = fopen(outName, "wb");
		fwrite(&wadfile, sizeof(wadfile), 1, wadout);

		// Init palettes
		g_playPal = W_ReadLumpName("PLAYPAL");
		g_titlePal = W_ReadLumpName("TITLEPAL");
		g_creditPal = W_ReadLumpName("CREDITPA");
		g_logoGTPal = W_ReadLumpName("PLOGOGT");
		g_logoIDPal = W_ReadLumpName("PLOGOID");
		g_logoSCPal = W_ReadLumpName("PLOGOSC");
		g_logoRAVPal = W_ReadLumpName("PLOGORAV");
		g_legalsPal = W_ReadLumpName("PLEGALS");

		g_begMusic = W_CheckNumForName("BLECHR");
		g_endMusic = W_CheckNumForName("STARTSNG");

		for (i = 0; i < numlumps; i++)
		{
			PrintfPorcentaje(i, numlumps - 1, true, getcoord.Y, "Decompressing %s...\t", argv[2]);
			CopyLump(i, decWadFormat, false);
		}

		if (decWadFormat) {
			// Add SFX Midis
			getcoord.Y++;
			for (i = 1; i < 246; i++) {
				PrintfPorcentaje(i, 246 - 2, true, getcoord.Y, "Add Sfx Midis...\t\t");
				char MidiName[8];
				sprintf(MidiName, "SFX_%03d", i);
				MusStartEffect(i);

				uint8_t* midiBuff = Song2Midi();
				int size = *(int*)midiBuff;
				fwrite(midiBuff + sizeof(int), sizeof(byte), size, wadout);

				W_AddLumpEntry(MidiName, size);
			}
		}

		setcolor(0x07);

		for (i = 0; i < numlumps; i++)
		{
			fwrite(&lumpinfo[i].filepos, sizeof(unsigned int), 1, wadout);
			fwrite(&lumpinfo[i].size, sizeof(unsigned int), 1, wadout);
			fwrite(&lumpinfo[i].name, sizeof(unsigned int) * 2, 1, wadout);
		}

		fseek(wadout, 0, SEEK_SET);
		wadfile.numlumps = numlumps;
		wadfile.infotableofs = offcnt;
		fwrite(&wadfile, sizeof(wadfile), 1, wadout);

		free(g_playPal);
		free(g_titlePal);
		free(g_creditPal);
		free(g_logoGTPal);
		free(g_logoIDPal);
		free(g_logoSCPal);
		free(g_logoRAVPal);
		free(g_legalsPal);

		free(lumpinfo);
		fclose(wadout);
		fclose(wadin);

		if (decWadFormat) {
			Sound_WriteSoundFont();
			Sound_FreePtrs();
		}	
	}

	setcolor(0x0A);
	printf("Completed\n");
	setcolor(0x07);

	return EXIT_SUCCESS;
}
