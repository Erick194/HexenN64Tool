// Emacs style mode select       -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: SndFont.c 1096 2012-03-31 18:28:01Z svkaiser $
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// $Author: svkaiser $
// $Revision: 1096 $
// $Date: 2012-03-31 11:28:01 -0700 (Sat, 31 Mar 2012) $
//
//
//-----------------------------------------------------------------------------
#ifdef RCSID
static const char rcsid[] =
    "$Id: SndFont.c 1096 2012-03-31 18:28:01Z svkaiser $";
#endif

#include <math.h>

#include "Sound.h"
#include "Sndfont.h"

#define _PAD4(x)	x += (4 - ((uint32_t) x & 3)) & 3
#define _PAD8(x)	x += (8 - ((uint32_t) x & 7)) & 7
#define _PAD16(x)	x += (16 - ((uint32_t) x & 15)) & 15

#define SF_NEWITEM(p, t, c) p = (t*)realloc((t*)p, sizeof(t) * ++c);

uint32_t nsampledata = 0;
soundfont_t soundfont;
sfdata_t *data;
sfpresetlist_t *list;
sfsample_t *sample;
sfpreset_t *preset;
sfinst_t *inst;

//
// UTILITES
//
static double mylog2(double x)
{
	return (log10(x) / log10(2));
}

//
// not really sure how they're calculated in the actual game
// but they have to be measured in milliseconds. wouldn't make sense
// if it wasn't
//
static double usectotimecents(int usec)
{
	double t = (double)usec / 1000.0;
	return (1200 * mylog2(t));
}

static double usectotimecentsHexen(int usec, double speed)
{
	//double t = ((double)usec / 255.f) * (6.25f * 1);
	//return (1200 * mylog2(t)) * speed;

	double t = ((double)usec * 3.2f) / 255.f;
	return (1200 * mylog2(t)) * speed;
}

static double pantopercent(int pan)
{
	double p = (double)((pan - 64) * 25) / 32.0f;
	return p / 0.1;
}

//
// Doom64 does some really weird stuff involving the root key, the
// currently played note and the sample's pitch correction.
// instead of mimicing what they're doing, I'll just correct
// the pitch and Update the root key directly
//
static int getoriginalpitch(int key, float pitch)
{
	return ((pitch / 100) - key) * -1;
}

static double attentopercent(int attenuation)
{
	double a = (double)((127 - attenuation) * 50) / 127.f;
	return a / 0.1;
}

//**************************************************************
//**************************************************************
//      SF_AddPreset
//**************************************************************
//**************************************************************

static int npresets = 0;
static int curpbag = 0;

void
SF_AddPreset(sfpheader_t * preset, char *name, int idx, int bank, int presetidx)
{
	SF_NEWITEM(preset->presets, sfpresetinfo_t, npresets);
	strncpy(preset->presets[idx].achPresetName, name, 20);
	preset->presets[idx].wPresetBagNdx = curpbag;
	preset->presets[idx].wPreset = presetidx;
	preset->presets[idx].dwGenre = 0;
	preset->presets[idx].dwLibrary = 0;
	preset->presets[idx].dwMorphology = 0;
	preset->presets[idx].wBank = bank;
	preset->size = 38 * npresets;
}

//**************************************************************
//**************************************************************
//      SF_AddInstrument
//**************************************************************
//**************************************************************

static int ninstruments = 0;
void SF_AddInstrument(sfinstheader_t * inst, char *name, int idx)
{
	SF_NEWITEM(inst->instruments, sfinstinfo_t, ninstruments);
	strncpy(inst->instruments[idx].achInstName, name, 20);
	inst->instruments[idx].wInstBagNdx = idx;
	inst->size += sizeof(sfinstinfo_t);
}

//**************************************************************
//**************************************************************
//      SF_AddInstrumentGenerator
//**************************************************************
//**************************************************************

static int igenpos = 0;
static int nigens = 0;
void
SF_AddInstrumentGenerator(sfigenheader_t * gen, generatorops_e op,
			  gentypes_t value)
{
	sfinstgen_t *igen;

	SF_NEWITEM(gen->info, sfinstgen_t, nigens);
	igen = &gen->info[nigens - 1];
	igen->sfGenOper = op;
	igen->genAmount = value;
	gen->size += sizeof(sfinstgen_t);
	igenpos++;
}

//**************************************************************
//**************************************************************
//      SF_AddPresetGenerator
//**************************************************************
//**************************************************************

static int pgenpos = 0;
static int npgens = 0;
void
SF_AddPresetGenerator(sfpgenheader_t * gen, generatorops_e op, gentypes_t value)
{
	sfpresetgen_t *pgen;

	SF_NEWITEM(gen->info, sfpresetgen_t, npgens);
	pgen = &gen->info[npgens - 1];
	pgen->sfGenOper = op;
	pgen->genAmount = value;
	gen->size += sizeof(sfpresetgen_t);
	pgenpos++;
}

//**************************************************************
//**************************************************************
//      SF_AddInstrumentBag
//**************************************************************
//**************************************************************

static int nibags = 0;
void SF_AddInstrumentBag(sfibagheader_t * bag)
{
	sfinstbag_t *instbag;

	SF_NEWITEM(bag->instbags, sfinstbag_t, nibags);
	instbag = &bag->instbags[nibags - 1];
	instbag->wInstGenNdx = igenpos;
	instbag->wInstModNdx = 0;
	bag->size += sizeof(sfinstbag_t);
}

//**************************************************************
//**************************************************************
//      SF_AddPresetBag
//**************************************************************
//**************************************************************

static int npbags = 0;
void SF_AddPresetBag(sfpbagheader_t * bag)
{
	sfpresetbag_t *presetbag;

	SF_NEWITEM(bag->presetbags, sfpresetbag_t, npbags);
	presetbag = &bag->presetbags[npbags - 1];
	presetbag->wGenNdx = pgenpos;
	presetbag->wModNdx = 0;
	bag->size += sizeof(sfpresetbag_t);

	curpbag = npbags;
}

//**************************************************************
//**************************************************************
//      SF_AddSample
//**************************************************************
//**************************************************************

static int nsamples = 0;
void
SF_AddSample(sfsample_t * sample, char *name, int size, int offset,
	     int loopid)
{
	sfsampleinfo_t *info;
	looptable_t *lptbl;

	SF_NEWITEM(sample->info, sfsampleinfo_t, nsamples);
	sample->size = 46 * nsamples;

	if (loopid != -1)
		lptbl = &looptable[loopid];

	info = &sample->info[nsamples - 1];
	strncpy(info->achSampleName, name, 20);
	info->byOriginalPitch = 60;
	info->chPitchCorrection = 0;
	info->dwSampleRate = 22050;
	info->sfSampleType = (loopid != -1);
	info->wSampleLink = 0;
	info->dwStart = offset / 2;
	info->dwEnd = (size / 2) + (offset / 2);

	if (loopid == -1) {
		info->dwStartloop = offset / 2;
		info->dwEndloop = ((size / 2) + (offset / 2)) - 1;
	} else {
		info->dwStartloop = (offset / 2) + lptbl->loopstart;
		info->dwEndloop = lptbl->loopend + (offset / 2);
	}
}

//**************************************************************
//**************************************************************
//      SF_AddSampleData
//**************************************************************
//**************************************************************

static uint32_t offset = 0;
void
SF_AddSampleData(soundfont_t * sf, cache in, int insize,
		 char *newname, int loopid)
{
	sfdata_t *data;

	data = &sf->data;
	data->size += (insize + 64);
	data->listsize = data->size + 12;
	data->wavdata = (byte *) realloc((byte *) data->wavdata, data->size);
	memset(data->wavdata + (data->size - 64), 0, 64);
	memcpy(data->wavdata + offset, in, insize);
	SF_AddSample(&sf->samples, newname, insize, offset, loopid);

	offset = data->size;
}

//**************************************************************
//**************************************************************
//      SF_SetupModulators
//**************************************************************
//**************************************************************

void SF_SetupModulators(void)
{
	preset->mod.sfModTransOper = 1;
	preset->mod.modAmount = 0;
	preset->mod.sfModAmtSrcOper = 0;
	preset->mod.sfModDestOper = 0;
	preset->mod.sfModSrcOper = 0;
	preset->mod.size = 0x0a;

	inst->mod.sfModTransOper = 3;
	inst->mod.modAmount = 0;
	inst->mod.sfModAmtSrcOper = 0;
	inst->mod.sfModDestOper = 0;
	inst->mod.sfModSrcOper = 0;
	inst->mod.size = 0x0a;
}

//**************************************************************
//**************************************************************
//      SF_FinalizeChunkSizes
//**************************************************************
//**************************************************************

void SF_FinalizeChunkSizes(void)
{
	list->listsize = 0x4C;
	list->listsize += preset->info.size;
	list->listsize += preset->bag.size;
	list->listsize += preset->mod.size;
	list->listsize += preset->gen.size;
	list->listsize += inst->info.size;
	list->listsize += inst->bag.size;
	list->listsize += inst->mod.size;
	list->listsize += inst->gen.size;
	list->listsize += sample->size;

	soundfont.filesize += list->listsize + 8 + data->listsize;
}

//**************************************************************
//**************************************************************
//      SF_CreatePresets
//**************************************************************
//**************************************************************

uint32_t totalpresets = 0;
uint32_t totalinstruments = 0;

void SF_CreatePresets(wavtable_t * wavtable)
{
	int i;
	int j;
	uint32_t banks = 0;
	uint32_t presetcount = 0;
	gentypes_t value;
	char name[20];

	//WGen_UpdateProgress("Building Soundfont Presets...");

	// envelopes
	//if (!envelopes.empty())
{
		for (int i = 0; i < cantidadEnvelopes; i++) {
			envelope_t evenNext = g_envelopes[i];

			//printf("\t  index %d ---------------\n", index);
			//printf("\t  bankIndex %d\n", bankIndex);
			//printf("\t  wave %d\n", evenNext.wave);
			//printf("\t  env_speed %d\n", evenNext.env_speed);
			//printf("\t  env_init_vol %d\n", evenNext.env_init_vol);
			//printf("\t  env_max_vol %d\n", evenNext.env_max_vol);
			//printf("\t  env_sustain_vol %d\n", evenNext.env_sustain_vol);
			//printf("\t  env_attack_speed %d\n", evenNext.env_attack_speed);
			//printf("\t  env_decay_speed %d\n", evenNext.env_decay_speed);
			//printf("\t  env_release_speed %d\n", evenNext.env_release_speed);

			banks = (i / 128);
			presetcount = (i & 127);
			sprintf(name, "PRESET_%03d", totalpresets);
			SF_AddPreset(&preset->info, name, totalpresets, banks, presetcount);

			wavtable_t* wt;

			SF_AddPresetBag(&preset->bag);
			value.wAmount = totalinstruments;
			SF_AddPresetGenerator(&preset->gen, GEN_INSTRUMENT, value);

			//sp = &subpatch[p->offset + j];
			wt = &wavtable[evenNext.wave];

			sprintf(name, "INST_%03d", totalinstruments);
			SF_AddInstrument(&inst->info, name, totalinstruments);
			SF_AddInstrumentBag(&inst->bag);

			//
			// min/max notes
			//
			value.ranges.byLo = 0;//sp->minnote;
			value.ranges.byHi = 127;//sp->maxnote;
			SF_AddInstrumentGenerator(&inst->gen, GEN_KEYRANGE,
				value);

			//
			// attenuation
			//
			/*if (sp->attenuation < 127) {
				value.wAmount =
					(int)attentopercent(sp->attenuation);
				SF_AddInstrumentGenerator(&inst->gen,
							  GEN_ATTENUATION,
							  value);
			}*/

			//
			// panning
			//
			/*if (sp->pan != 64) {
				value.wAmount = (int)pantopercent(sp->pan);
				SF_AddInstrumentGenerator(&inst->gen, GEN_PAN,
							  value);
			}*/

			//
			// attack time
			//
			//if (evenNext.env_attack_speed > 1) {
				value.wAmount =
					(int)usectotimecentsHexen(evenNext.env_attack_speed, evenNext.env_speed);
				SF_AddInstrumentGenerator(&inst->gen,
							  GEN_VOLENVATTACK,
							  value);
			//}

			//
			// decay  time
			//
			//if (evenNext.env_decay_speed > 1) {
				value.wAmount =
					(int)usectotimecentsHexen(evenNext.env_decay_speed, (double)evenNext.env_speed*1.5);
				SF_AddInstrumentGenerator(&inst->gen,
					GEN_VOLENVDECAY,
					value);
			//}

			//
			// sustain vol
			//
			//if (evenNext.env_sustain_vol > 1) {
				value.wAmount = (int)attentopercent(evenNext.env_sustain_vol);
				SF_AddInstrumentGenerator(&inst->gen,
					GEN_VOLENVSUSTAIN,
					value);
			//}


			//
			// release time
			//
			//if (evenNext.env_release_speed > 1) {
				value.wAmount =
					(int)usectotimecentsHexen(evenNext.env_release_speed, evenNext.env_speed);
				SF_AddInstrumentGenerator(&inst->gen,
							  GEN_VOLENVRELEASE,
							  value);
			//}
			//
			// sample loops
			//
			if (wt->loopOffset != 0) {
				value.wAmount = 1;
				SF_AddInstrumentGenerator(&inst->gen,
					GEN_SAMPLEMODE,
					value);
			}

			//
			// root key override
			//
			value.wAmount = 60;
			SF_AddInstrumentGenerator(&inst->gen,
						  GEN_OVERRIDEROOTKEY, value);

			//
			// tune coarse
			//
			value.wAmount = (mus_basenote[evenNext.wave] - 5 + evenNext.transpose) + value.wAmount;
			SF_AddInstrumentGenerator(&inst->gen,
				GEN_COARSETUNE, value);

			//
			// tune fine
			//
			if (mus_detuneInt[evenNext.wave] != 0) {
				value.shAmount = mus_detuneInt[evenNext.wave];
				SF_AddInstrumentGenerator(&inst->gen,
					GEN_FINETUNE, value);
			}

			//
			// sample id
			//
			value.wAmount = evenNext.wave;
			SF_AddInstrumentGenerator(&inst->gen, GEN_SAMPLEID,
				value);

			totalinstruments++;
			//}

			totalpresets++;
		}
	}


	SF_AddPreset(&preset->info, "EOP", totalpresets, 0, ++banks);
	SF_AddPresetBag(&preset->bag);
	value.wAmount = 0;
	SF_AddPresetGenerator(&preset->gen, GEN_INSTRUMENT, value);

	SF_AddInstrument(&inst->info, "EOI", totalinstruments);
	SF_AddInstrumentBag(&inst->bag);
	value.wAmount = 0;
	SF_AddInstrumentGenerator(&inst->gen, GEN_SAMPLEID, value);
}


void SF_Write(FILE* handle, void* source, int length)
{
	int count;

	count = fwrite(source, sizeof(byte), length, handle);

	if (count < length) {
		printf("File_Write: Miscounted %i < %i", count, length);
	}
}

//**************************************************************
//**************************************************************
//      SF_WriteSoundFont
//**************************************************************
//**************************************************************

void SF_WriteSoundFont(void)
{
	FILE *handle;
	int i;
	char outFile [64];

    sprintf(outFile, "hexen64snd.sf2");
	handle = fopen(outFile, "wb");

	SF_Write(handle, soundfont.RIFF, 4);
	SF_Write(handle, &soundfont.filesize, 4);
	SF_Write(handle, soundfont.sfbk, 4);
	SF_Write(handle, soundfont.LIST, 4);
	SF_Write(handle, &soundfont.listsize, 4);
	SF_Write(handle, soundfont.INFO, 4);
	SF_Write(handle, &soundfont.version, sizeof(sfversion_t));
	SF_Write(handle, soundfont.name.INAM, 4);
	SF_Write(handle, &soundfont.name.size, 4);
	SF_Write(handle, soundfont.name.name, soundfont.name.size);
	SF_Write(handle, data->LIST, 4);
	SF_Write(handle, &data->listsize, 4);
	SF_Write(handle, data->sdta, 4);
	SF_Write(handle, data->smpl, 4);
	SF_Write(handle, &data->size, 4);
	SF_Write(handle, data->wavdata, data->size);
	SF_Write(handle, list->LIST, 4);
	SF_Write(handle, &list->listsize, 4);
	SF_Write(handle, list->pdta, 4);
	SF_Write(handle, preset->info.phdr, 4);
	SF_Write(handle, &preset->info.size, 4);

	for (i = 0; i < npresets; i++) {
		SF_Write(handle, preset->info.presets[i].achPresetName, 20);
		SF_Write(handle, &preset->info.presets[i].wPreset, 2);
		SF_Write(handle, &preset->info.presets[i].wBank, 2);
		SF_Write(handle, &preset->info.presets[i].wPresetBagNdx, 2);
		SF_Write(handle, &preset->info.presets[i].dwLibrary, 4);
		SF_Write(handle, &preset->info.presets[i].dwGenre, 4);
		SF_Write(handle, &preset->info.presets[i].dwMorphology, 4);
	}

	SF_Write(handle, preset->bag.pbag, 4);
	SF_Write(handle, &preset->bag.size, 4);
	SF_Write(handle, preset->bag.presetbags,
		   sizeof(sfpresetbag_t) * npbags);
	SF_Write(handle, preset->mod.pmod, 4);
	SF_Write(handle, &preset->mod.size, 4);
	SF_Write(handle, &preset->mod.sfModSrcOper, 2);
	SF_Write(handle, &preset->mod.sfModDestOper, 2);
	SF_Write(handle, &preset->mod.modAmount, 2);
	SF_Write(handle, &preset->mod.sfModAmtSrcOper, 2);
	SF_Write(handle, &preset->mod.sfModTransOper, 2);
	SF_Write(handle, preset->gen.pgen, 4);
	SF_Write(handle, &preset->gen.size, 4);
	SF_Write(handle, preset->gen.info, sizeof(sfpresetgen_t) * npgens);
	SF_Write(handle, inst->info.inst, 4);
	SF_Write(handle, &inst->info.size, 4);
	SF_Write(handle, inst->info.instruments,
		   sizeof(sfinstinfo_t) * ninstruments);
	SF_Write(handle, inst->bag.ibag, 4);
	SF_Write(handle, &inst->bag.size, 4);
	SF_Write(handle, inst->bag.instbags, sizeof(sfinstbag_t) * nibags);
	SF_Write(handle, inst->mod.imod, 4);
	SF_Write(handle, &inst->mod.size, 4);
	SF_Write(handle, &inst->mod.sfModSrcOper, 2);
	SF_Write(handle, &inst->mod.sfModDestOper, 2);
	SF_Write(handle, &inst->mod.modAmount, 2);
	SF_Write(handle, &inst->mod.sfModAmtSrcOper, 2);
	SF_Write(handle, &inst->mod.sfModTransOper, 2);
	SF_Write(handle, inst->gen.igen, 4);
	SF_Write(handle, &inst->gen.size, 4);
	SF_Write(handle, inst->gen.info, sizeof(sfinstgen_t) * nigens);
	SF_Write(handle, sample->shdr, 4);
	SF_Write(handle, &sample->size, 4);

	for (i = 0; i < nsamples; i++) {
		SF_Write(handle, sample->info[i].achSampleName, 20);
		SF_Write(handle, &sample->info[i].dwStart, 4);
		SF_Write(handle, &sample->info[i].dwEnd, 4);
		SF_Write(handle, &sample->info[i].dwStartloop, 4);
		SF_Write(handle, &sample->info[i].dwEndloop, 4);
		SF_Write(handle, &sample->info[i].dwSampleRate, 4);
		SF_Write(handle, &sample->info[i].byOriginalPitch, 1);
		SF_Write(handle, &sample->info[i].chPitchCorrection, 1);
		SF_Write(handle, &sample->info[i].wSampleLink, 2);
		SF_Write(handle, &sample->info[i].sfSampleType, 2);
	}

	fclose(handle);

	free(sample->info);
	free(data->wavdata);
	free(preset->info.presets);
	free(preset->bag.presetbags);
	free(preset->gen.info);
	free(inst->info.instruments);
	free(inst->bag.instbags);
	free(inst->gen.info);

	//WGen_Printf("Sucessfully created %s", outFile);
}

//**************************************************************
//**************************************************************
//      SF_Setup
//**************************************************************
//**************************************************************

void SF_Setup(void)
{
	//printf("SF_Setup\n");
	data = &soundfont.data;
	list = &soundfont.presetlist;
	sample = &soundfont.samples;
	preset = &soundfont.presetlist.preset;
	inst = &soundfont.presetlist.inst;

	//
	// setup tags
	//
	strncpy(soundfont.RIFF, "RIFF", 4);
	strncpy(soundfont.sfbk, "sfbk", 4);
	strncpy(soundfont.LIST, "LIST", 4);
	strncpy(soundfont.INFO, "INFO", 4);
	strncpy(soundfont.data.LIST, "LIST", 4);
	strncpy(soundfont.data.sdta, "sdta", 4);
	strncpy(soundfont.data.smpl, "smpl", 4);
	strncpy(soundfont.version.ifil, "ifil", 4);
	strncpy(soundfont.name.INAM, "INAM", 4);
	strncpy(list->LIST, "LIST", 4);
	strncpy(list->pdta, "pdta", 4);
	strncpy(preset->info.phdr, "phdr", 4);
	strncpy(preset->gen.pgen, "pgen", 4);
	strncpy(preset->bag.pbag, "pbag", 4);
	strncpy(preset->mod.pmod, "pmod", 4);
	strncpy(inst->info.inst, "inst", 4);
	strncpy(inst->gen.igen, "igen", 4);
	strncpy(inst->bag.ibag, "ibag", 4);
	strncpy(inst->mod.imod, "imod", 4);
	strncpy(soundfont.samples.shdr, "shdr", 4);

	//
	// version
	//
	soundfont.version.size = 4;
	soundfont.version.wMajor = 2;
	soundfont.version.wMinor = 1;

	//
	// name
	//
	soundfont.name.size = strlen("hexen64snd.sf2") + 1;
	_PAD4(soundfont.name.size);
	soundfont.name.name = (char *)malloc(soundfont.name.size);
	strncpy(soundfont.name.name, "hexen64snd.sf2", soundfont.name.size);

	//
	// initalize chunks
	//
	soundfont.listsize = 24 + soundfont.name.size;
	soundfont.filesize = 20 + soundfont.listsize;

	sample->size = 0;
	sample->info = (sfsampleinfo_t *)malloc(1);

	data->size = 0;
	data->wavdata = (byte *)malloc(1);

	preset->info.size = 0;
	preset->bag.size = 0;
	preset->gen.size = 0;
	preset->info.presets = (sfpresetinfo_t *) malloc(1);
	preset->bag.presetbags = (sfpresetbag_t *) malloc(1);
	preset->gen.info = (sfpresetgen_t *) malloc(1);

	inst->info.size = 0;
	inst->bag.size = 0;
	inst->gen.size = 0;
	inst->info.instruments = (sfinstinfo_t *) malloc(1);
	inst->bag.instbags = (sfinstbag_t *) malloc(1);
	inst->gen.info = (sfinstgen_t *) malloc(1);
}
