
#include "Sound.h"

extern void Error(const char* s, ...);
extern int SwapInt(int value);
extern unsigned short SwapUShort(unsigned short value);
extern short SwapShort(short value);
extern void PrintfPorcentaje(int count, int Prc, bool draw, int ypos, const char* s, ...);
extern COORD getcoord;

#ifdef USE_SOUNDFONTS
#include "Sndfont.h"
#endif

#define WAV_HEADER_SIZE     0x2C

wavtable_t* sfx;
looptable_t* looptable;
predictor_t* predictors;

cache* wavtabledata;
byte* sfxdata;

static uint32_t newbankoffset = 0;

static const short itable[16] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	-8, -7, -6, -5, -4, -3, -2, -1,
};

static uint8_t fillbuffer[32768];
uint8_t* midiBuff = nullptr;
vec_miditrack_t miditrack;

//
// lump names for midi tracks
//
const char* songlumpnames[] = {
	"BLECHR.dat",
	"BONESR.dat",
	"BORKR.dat",
	"CHAP_1R.dat",
	"CHAP_3R.dat",
	"CHAP_4R.dat",
	"CHESS.dat",
	"CHIPPYR.dat",
	"CRUCIBR.dat",
	"CRYPTR.dat",
	"DEEPR.dat",
	"FALCONR.dat",
	"FANTAR.dat",
	"FOOJAR.dat",
	"FORTR.dat",
	"FUBASR.dat",
	"GROVER.dat",
	"HALL.dat",
	"HEXEN.dat",
	"HUB.dat",
	"JACHR.dat",
	"LEVELR.dat",
	"OCTOR.dat",
	"ORB.dat",
	"PERCR.dat",
	"RITHMR.dat",
	"SECRETR.dat",
	"SIMONR.dat",
	"SIXATER.dat",
	"STALKR.dat",
	"SWAMPR.dat",
	"VOIDR.dat",
	"WINNOWR.dat",
	"WOBABYR.dat",
	"WUTZITR.dat",
	"WINDSONG.dat",
	"STARTSNG.dat",
	NULL
};

boolean compareDeltaTime(const midiEvent_t& a, const midiEvent_t& b) {
	return a.deltaTime < b.deltaTime;
}

void add_event(const int channel, uint32_t deltaTime, const uint8_t* buf, const bool checkNote = false) {
	const midiEvent_t newEvent = { deltaTime, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7] };

	if (checkNote) {
		// Buscar el evento previo con el mismo delta time
		for (size_t i = 0; i < miditrack.midi_events[channel].size(); i++) {
			if (miditrack.midi_events[channel][i].deltaTime == deltaTime) {
				// Si el evento previo es un "Note On" (Status Byte: 0x90)
				if (miditrack.midi_events[channel][i].buf[0] == MIDI_NOTE_ON) {
					// Insertar el nuevo evento en la posición del "Note On"
					miditrack.midi_events[channel].insert(miditrack.midi_events[channel].begin() + i, newEvent);
					break;
				}
			}
		}
	}
	else {
		miditrack.midi_events[channel].push_back(newEvent);
	}

	// Ordenar los eventos MIDI en la pista por deltaTime
	std::sort(miditrack.midi_events[channel].begin(), miditrack.midi_events[channel].end(), compareDeltaTime);
}

int WriteVarLen(unsigned long value, uint8_t* buff)
{
	unsigned long buffer;
	int len = 0;

	buffer = value & 0x7F;

	while ((value >>= 7))
	{
		buffer <<= 8;
		buffer |= ((value & 0x7F) | 0x80);
	}

	while (TRUE)
	{
		buff[len++] = (uint8_t)(buffer & 0xff);
		if (buffer & 0x80)
			buffer >>= 8;
		else
			break;
	}

	return len;
}

uint8_t* createMidi(const char* filename)
{
	midiheader_t mthd;
	miditrack_t* mtrk;
	uint8_t* buff;
	int tracksize;
	FILE* pFile;

	if (filename != nullptr) {
		printf("createMidi %s\n", filename);
	}

	memcpy(mthd.header, "MThd", 4);

	int num_tracks = 0;

	for (int i = 0; i < 16; i++) {
		if (miditrack.midi_events[i].empty()) {
			continue;
		}
		num_tracks++;
	}

	//printf("num_tracks %d\n",num_tracks);
	//_getch();

	mthd.chunksize = SwapInt(MIDI_HEADER_LENGTH);
	mthd.type = SwapShort(MIDI_TYPE_1);
	mthd.ntracks = SwapShort(num_tracks);
	mthd.size = MIDI_HEADER_BYTES;
	mthd.tracks = (miditrack_t*)malloc(sizeof(miditrack_t) * num_tracks);

	for (int i = 0; i < num_tracks; i++) {
		mtrk = &mthd.tracks[i];
		mtrk->length = 0;
	}

	mthd.delta = SwapShort(48); // default

	boolean initTempo = FALSE;
	int track = 0;
	int chan;
	for (int i = 0; i < 16; i++)
	{
		if (miditrack.midi_events[i].empty()) {
			continue;
		}

		// Ajusta el MIDI_END_OF_TRACK
		if (miditrack.midi_events[i].size()) {

			uint8_t buf[8];
			midiEvent_t a = miditrack.midi_events[i].at(miditrack.midi_events[i].size() - 1);
			buf[0] = MIDI_META_EVENT;
			buf[1] = MIDI_END_OF_TRACK;

			//printf("buf[0] %d\n", buf[0]);
			//printf("buf[1] %d\n", buf[1]);

			add_event(i, a.deltaTime, buf);
		}

		//std::sort(miditrack.midi_events[i].begin(), miditrack.midi_events[i].end(), compareDeltaTime);

		mtrk = &mthd.tracks[track];

		memcpy(mtrk->header, "MTrk", 4);

		memset(fillbuffer, 0, 32768);
		tracksize = 0;
		buff = fillbuffer;

		char unkevent[32];
		int len, p;

		unsigned long olddeltatime = 0, value = 0;
		for (int j = 0; j < miditrack.midi_events[i].size(); j++) {

			midiEvent_t midi_event = miditrack.midi_events[i].at(j);

			/*printf("-----------------------------------\n");
			printf("midi_event track [%d], deltaTime[%d]\n buff[", i, midi_event.deltaTime);

			for (int k = 0; k < 8; k++) {
				printf("%d,", midi_event.buf[k]);
			}

			printf("]\n");*/

			value = (midi_event.deltaTime - olddeltatime) / 256;
			olddeltatime = midi_event.deltaTime;
			int len = WriteVarLen(value, buff);
			buff += len;
			tracksize += len;

			if (midi_event.buf[1] == MIDI_END_OF_TRACK) {
				//printf("MIDI_END_OF_TRACK\n");
				*buff++ = MIDI_META_EVENT;
				*buff++ = (0x7f & midi_event.buf[1]);
				tracksize += 2;
			}

			if (midi_event.buf[1] == MIDI_TEMPO_EVENT) {
				//printf("MIDI_TEMPO_EVENT\n");
				*buff++ = midi_event.buf[0];
				*buff++ = midi_event.buf[1];
				*buff++ = midi_event.buf[2];
				*buff++ = midi_event.buf[3];
				*buff++ = midi_event.buf[4];
				*buff++ = midi_event.buf[5];
				tracksize += 6;
			}

			switch (midi_event.buf[0]) {

			case MIDI_PROGRAM_CHANGE:
				//printf("MIDI_PROGRAM_CHANGE\n");
				*buff++ = (MIDI_PROGRAM_CHANGE | (0x0f & midi_event.buf[1]));
				*buff++ = (0x7f & midi_event.buf[2]);
				tracksize += 2;
				break;

			case MIDI_CONTROL_CHANGE:
				//printf("MIDI_CONTROL_CHANGE\n");
				*buff++ = (MIDI_CONTROL_CHANGE | (0x0f & midi_event.buf[1]));
				*buff++ = (0x7f & midi_event.buf[2]);
				*buff++ = (0x7f & midi_event.buf[3]);
				tracksize += 3;
				break;

			case MIDI_NOTE_ON:
				//printf("MIDI_NOTE_ON\n");
				*buff++ = (MIDI_NOTE_ON | (0x0f & midi_event.buf[1]));
				*buff++ = (0x7f & midi_event.buf[2]);
				*buff++ = (0x7f & midi_event.buf[3]);
				tracksize += 3;
				break;

			case MIDI_NOTE_OFF:
				//printf("MIDI_NOTE_OFF\n");
				*buff++ = (MIDI_NOTE_ON | (0x0f & midi_event.buf[1]));
				*buff++ = (0x7f & midi_event.buf[2]);
				*buff++ = (0x7f & midi_event.buf[3]);
				tracksize += 3;
				break;

			case MIDI_PITCH_WHEEL:
				//printf("MIDI_PITCH_WHEEL\n");
				*buff++ = (MIDI_PITCH_WHEEL | (0x0f & midi_event.buf[1]));
				*buff++ = (0x7f & midi_event.buf[2]);
				*buff++ = (0x7f & midi_event.buf[3]);
				tracksize += 3;
				break;

			default:
				//PRINTSTRING(midi_event.buf[0]);
				break;
			}

		}

		tracksize++;

		mtrk->length = SwapInt(tracksize);
		mtrk->data = (uint8_t*)malloc(tracksize);
		memcpy(mtrk->data, fillbuffer, tracksize);


		mthd.size += tracksize + 8;
		track++;
	}

	for (int i = 0; i < 16; i++) {
		miditrack.midi_events[i].clear();
	}

	mthd.size = SwapInt(mthd.size);

	uint8_t* midiBufftmp;
	uint32_t midiSize;

	midiSize = 0;
	midiSize += sizeof(midiheader_t) - 8; // header
	for (int i = 0; i < track; i++) {
		if (mthd.tracks[i].length)
		{
			midiSize += 4; // header
			midiSize += 4; // length
			midiSize += SwapInt(mthd.tracks[i].length); // data
		}
	}

	if (midiBuff != nullptr) {
		free(midiBuff);
		midiBuff = nullptr;
	}

	midiBuff = (uint8_t*)malloc(midiSize + sizeof(int));
	memset(midiBuff, 0, midiSize + sizeof(int));

	if (filename != nullptr) {
		midiBufftmp = midiBuff;
	}
	else {
		*(int*)midiBuff = midiSize; // guarda el tamaño del archivo temporalmente
		midiBufftmp = midiBuff + 4; // file size tmp
	}

	memmove(midiBufftmp, mthd.header, sizeof(midiheader_t) - 8);
	midiBufftmp += sizeof(midiheader_t) - 8; // header

	for (int i = 0; i < track; i++) {
		if (mthd.tracks[i].length)
		{
			memmove(midiBufftmp, mthd.tracks[i].header, 4);
			midiBufftmp += 4; // header

			memmove(midiBufftmp, &mthd.tracks[i].length, 4);
			midiBufftmp += 4; // length

			memmove(midiBufftmp, mthd.tracks[i].data, SwapInt(mthd.tracks[i].length)); // data
			midiBufftmp += SwapInt(mthd.tracks[i].length);
		}
	}

	for (int i = 0; i < track; i++) {
		if (mthd.tracks[i].length)
		{
			free(mthd.tracks[i].data);
		}
	}
	free(mthd.tracks);

	if (filename != nullptr) {
		char midiname[256];

		// remove ext
		for (int i = 0; i < sizeof(midiname); i++) {
			midiname[i] = *filename++;

			if (*filename == '.') {
				midiname[i + 1] = '\0';
				break;
			}
		}

		sprintf_s(midiname, sizeof(midiname), "%s.mid", midiname);
		FILE* midi = fopen(midiname, "wb");
		fwrite(midiBuff, sizeof(uint8_t), midiSize, midi);
		fclose(midi);
	}

	return midiBuff;
}

//**************************************************************
//**************************************************************
//      N64 Sound Tool aka libmus.lib
//**************************************************************
//**************************************************************
static channel_t mus_channels[MAXCHANNELS];	// music player channels
char* mus_basenote;							// samples base notes
float* mus_detune;							// samples detune offsets
int* mus_detuneInt;							// samples detune offsets
byte* mus_effects[246];						// address of sound effects
static long mus_random_seed;				// random number seed value
static uint32_t mus_num_of_samples;			// number of samples

std::vector<envelope_t> envelopes;
envelope_t curEnvelope;

/* default velocity map */
static char velocity_map[128] =
{
  30,30,30,30,30,30,30,30,          // 1-8
  35,35,35,35,35,35,35,35,          // 9-16
  40,40,40,40,40,40,40,40,          // 17-24
  45,45,45,45,45,45,45,45,          // 25-32
  50,50,50,50,50,50,50,50,          // 33-40
  55,55,55,55,55,55,55,55,          // 41-48
  60,60,60,60,60,60,60,60,          // 49-56
  65,65,65,65,65,65,65,65,          // 57-64
  70,70,70,70,70,70,70,70,          // 65-72
  75,75,75,75,75,75,75,75,          // 73-80
  80,80,80,80,80,80,80,80,          // 81-88
  90,90,90,90,90,90,90,90,          // 89-96
  95,95,95,95,95,95,95,95,          // 97-104
  100,100,100,100,105,105,105,105,  // 105-112
  110,110,110,110,115,115,115,115,  // 113-120
  115,115,115,115,120,120,124,127   // 121-128
};

//**************************************************************
//**************************************************************
//      add_envelope
//**************************************************************
//**************************************************************

int add_envelope(const envelope_t& envelope)
{
	std::vector<envelope_t>::iterator it;
	int index = 0;
	for (it = envelopes.begin(); it != envelopes.end(); it++) {
		const envelope_t evenNext = *it;
		if ((evenNext.wave == envelope.wave) &&
			(evenNext.transpose == envelope.transpose) &&
			(evenNext.env_speed == envelope.env_speed) &&
			(evenNext.env_init_vol == envelope.env_init_vol) &&
			(evenNext.env_max_vol == envelope.env_max_vol) &&
			(evenNext.env_sustain_vol == envelope.env_sustain_vol) &&
			(evenNext.env_attack_speed == envelope.env_attack_speed) &&
			(evenNext.env_decay_speed == envelope.env_decay_speed) &&
			(evenNext.env_release_speed == envelope.env_release_speed)) {

			return index; // bankIndex
		}
		index++;
	}

	envelopes.push_back(envelope);
	return index; // bankIndex
}

// Declarar un puntero global para un arreglo dinámico de estructuras envelope_t
envelope_t* g_envelopes = nullptr;
int cantidadEnvelopes = 0;

// Función para agregar un envelope al arreglo dinámico si no existe y devolver el índice
int agregarEnvelope(const envelope_t& nuevoEnvelope) {
	// Verificar si el envelope ya existe
	for (int i = 0; i < cantidadEnvelopes; i++) {
		if (memcmp(&g_envelopes[i], &nuevoEnvelope, sizeof(envelope_t)) == 0) {
			return i;
		}
	}

	// Aumentar la cantidad de envelopes
	cantidadEnvelopes++;

	// Asignar memoria para el nuevo arreglo dinámico
	envelope_t* nuevoArreglo = new envelope_t[cantidadEnvelopes];

	// Copiar los elementos anteriores al nuevo arreglo
	for (int i = 0; i < cantidadEnvelopes - 1; i++) {
		nuevoArreglo[i] = g_envelopes[i];
	}

	// Agregar el nuevo envelope al final del nuevo arreglo
	nuevoArreglo[cantidadEnvelopes - 1] = nuevoEnvelope;

	// Liberar la memoria del antiguo arreglo y apuntar al nuevo
	delete[] g_envelopes;
	g_envelopes = nuevoArreglo;

	// Devolver el índice del envelope recién agregado
	return cantidadEnvelopes - 1;
}

//**************************************************************
//**************************************************************
//      __MusIntRandom
//**************************************************************
//**************************************************************

static int __MusIntRandom(int range)
{
	long temp;
	int x;
	float f;

	for (x = 0; x < 8; x++)
	{
		temp = mus_random_seed & 0x48000000;
		mus_random_seed = mus_random_seed << 1;
		if (temp == 0x48000000 || temp == 0x08000000)
			mus_random_seed = mus_random_seed | 1;
	}
	f = (float)(mus_random_seed) / (1 << 16);
	f /= (1 << 16);
	return ((int)((float)(range)*f));
}

//**************************************************************
//**************************************************************
//      __MusIntInitialiseChannel
//**************************************************************
//**************************************************************

static void __MusIntInitialiseChannel(channel_t* cp)
{
	unsigned char old_playing, * work_ptr;
	int i;

	// disable chan processing 1st!!!
	cp->pdata = NULL;
	old_playing = cp->playing;

	// zero out chan
	work_ptr = (unsigned char*)cp;
	for (i = 0; i < sizeof(channel_t); i++)
		*work_ptr++ = 0;

	// set none zero values
	cp->old_volume = 0xffff;
	cp->old_reverb = 0xff;
	cp->old_pan = 0xff;
	cp->old_frequency = 99.9;

	cp->channel_tempo = cp->channel_tempo_save = 96 * 256 / 60/*mus_vsyncs_per_second*/;

	cp->length = 1;

	cp->velocity_on = 0; // now defaults to off! (was 1)
	cp->default_velocity = 127;
	cp->volume = 127;
	cp->pitchbend = 64;
	cp->bendrange = 2;
	cp->pan = 64;

	cp->cont_vol_repeat_count = 1;
	cp->cont_pb_repeat_count = 1;

	cp->stopping = -1;

	// new volume and pan scales
	cp->volscale = 0x80;
	cp->panscale = 0x80;
	cp->temscale = 0x80;

	// restore chan status flag
	cp->playing = old_playing;

	cp->note_end_frame = 0;
	cp->note_start_frame = 0;
	cp->volume_frame = 0;
	cp->freqoffset = 0;
}

//**************************************************************
//**************************************************************
//      Fdefa - set envelope
//**************************************************************
//**************************************************************

static unsigned char* Fdefa(channel_t* cp, unsigned char* ptr)
{
	char value;

	// get envelope speed...
	value = *ptr++;
	if (value == 0)	// cannot be zero!!!
		value = 1;
	cp->env_speed = value;

	// get envelope initial volume level...
	cp->env_init_vol = *ptr++;

	// get attack speed...
	value = *ptr++;
	if (value == 0)
		value = 1;
	cp->env_attack_speed = value;
	cp->env_attack_calc = 1.0 / ((float)value);

	// get peak volume...
	cp->env_max_vol = *ptr++;

	// get decay speed...
	value = *ptr++;
	if (value == 0)
		value = 1;
	cp->env_decay_speed = value;
	cp->env_decay_calc = 1.0 / ((float)value);

	// get sustain volume level...
	cp->env_sustain_vol = *ptr++;

	// get release speed...
	value = *ptr++;
	if (value == 0)
		value = 1;
	cp->env_release_speed = value;
	cp->env_release_calc = 1.0 / ((float)value);

	/*printf("\t  wave %d\n", cp->wave);
	printf("\t  env_speed %d\n", cp->env_speed);
	printf("\t  env_init_vol %d\n", cp->env_init_vol);
	printf("\t  env_max_vol %d\n", cp->env_max_vol);
	printf("\t  env_sustain_vol %d\n", cp->env_sustain_vol);
	printf("\t  env_attack_speed %d\n", cp->env_attack_speed);
	printf("\t  env_decay_speed %d\n", cp->env_decay_speed);
	printf("\t  env_release_speed %d\n", cp->env_release_speed);
	printf("\t  env_attack_calc %f\n", cp->env_attack_calc);
	printf("\t  env_decay_calc %f\n", cp->env_decay_calc);
	printf("\t  env_release_calc %f\n", cp->env_release_calc);*/

	curEnvelope.env_speed = cp->env_speed;
	curEnvelope.env_init_vol = cp->env_init_vol;
	curEnvelope.env_max_vol = cp->env_max_vol;
	curEnvelope.env_sustain_vol = cp->env_sustain_vol;
	curEnvelope.env_attack_speed = cp->env_attack_speed;
	curEnvelope.env_decay_speed = cp->env_decay_speed;
	curEnvelope.env_release_speed = cp->env_release_speed;
	/*curEnvelope = {0, 0, 0, cp->env_speed,
							cp->env_init_vol, cp->env_max_vol, cp->env_sustain_vol,
							cp->env_attack_speed, cp->env_decay_speed, cp->env_release_speed };*/
	return ptr;
}

//**************************************************************
//**************************************************************
//      MusStartSong
//**************************************************************
//**************************************************************
char MidiName[64];

static const char* curSongName = nullptr;
void MusStartSong(uint8_t* data, const char *fileName) {
	FILE* song;
	song_Header_t sheader;
	song_t* song_addr;
	unsigned long* work_addr;
	int i, count;
	channel_t* cp;
	curSongName = nullptr;

	if (fileName != nullptr && data == nullptr) {
		song = fopen(fileName, "rb");
		if (!song) {
			Error("Cannot open %s\n", fileName);
		}

		sprintf(MidiName, "MIDIS/%s", fileName);
		curSongName = MidiName;

		fseek(song, 0, SEEK_END);
		int size = ftell(song);
		fseek(song, 0, SEEK_SET);

		data = (uint8_t*)malloc(size);

		fread(data, sizeof(uint8_t), size, song);

		fclose(song);
		printf("fileName %s\n", fileName);
	}

	song_addr = (song_t*)data;

	work_addr = (unsigned long*)data;
	count = 16;//SwapInt(*work_addr++);

	//work_addr++; // count

	int chan_addr = SwapInt(*work_addr++);
	int vol_addr = SwapInt(*work_addr++);
	//work_addr++; // pitch
	int env_addr = SwapInt(*work_addr++);
	int drm_addr = SwapInt(*work_addr++);

	//printf("chan_addr %d\n", chan_addr);
	//printf("vol_addr %d\n", vol_addr);
	//printf("env_addr %d\n", env_addr);
	//printf("drm_addr %d\n", drm_addr);

	// remap song structure: chan ptrs
	work_addr = (unsigned long*)(data + chan_addr);
	for (int i = 0; i < count; i++) {
		song_addr->ChannelData[i] = NULL;
		int addr = SwapInt(work_addr[i]);
		if (addr) {
			song_addr->ChannelData[i] = (char*)(data + addr);
		}
	}

	// remap song structure: volume ptrs
	work_addr = (unsigned long*)(data + vol_addr);
	for (int i = 0; i < count; i++) {
		song_addr->VolumeData[i] = NULL;
		int addr = SwapInt(work_addr[i]);
		if (addr) {
			song_addr->VolumeData[i] = (char*)(data + addr);
		}
	}

	// remap song structure: envelope ptr
	work_addr = (unsigned long*)(data + env_addr);
	if (SwapInt(work_addr[0])) {
		song_addr->EnvelopeData = (char*)(data + env_addr);
	}
	else {
		song_addr->EnvelopeData = NULL;
	}

	// remap song structure: drum ptr
	work_addr = (unsigned long*)(data + drm_addr);
	if (SwapInt(work_addr[0])) {
		song_addr->DrumData = (unsigned long*)(data + drm_addr);
	}
	else {
		song_addr->DrumData = NULL;
	}

	for (int i = 0; i < count; i++) {
		if (song_addr->ChannelData[i] != NULL) {
			cp = &mus_channels[i];
			__MusIntInitialiseChannel(cp);
			cp->song_addr = song_addr;
			cp->pvolume = cp->pvolumebase = (unsigned char*)song_addr->VolumeData[i];
			// pdata must be set last to avoid processing clash
			cp->pdata = cp->pbase = (unsigned char*)song_addr->ChannelData[i];
		}
	}
}

//**************************************************************
//**************************************************************
//      MusStartEffect
//**************************************************************
//**************************************************************

void MusStartEffect(int number) {
	channel_t* cp;

	for (int i = 0; i < MAXCHANNELS; i++) {
		cp = &mus_channels[i];
		__MusIntInitialiseChannel(cp);
	}

	sprintf(MidiName, "SFX/SFX%03d", number);
	//curSongName = MidiName;
	curSongName = nullptr;

	cp = &mus_channels[0];
	cp->IsFX = number;
	cp->pvolume = NULL;
	cp->pvolumebase = NULL;
	// pdata must be set last to avoid processing clash
	cp->pdata = cp->pbase = mus_effects[number];
}

static float __MusIntPowerOf2(float x) {
	float x2;

	if (x == 0)
		return 1;

	if (x > 0)
	{
		x2 = x * x;
		return (1 + (x * .693147180559945)
			+ (x2 * .240226506959101)
			+ (x2 * x * 5.55041086648216E-02)
			+ (x2 * x2 * 9.61812910762848E-03)
			+ (x2 * x2 * x * 1.33335581464284E-03)
			+ (x2 * x2 * x2 * 1.54035303933816E-04)
			);
	}
	else
	{
		x = -x;
		x2 = x * x;
		return (1 / (1 + (x * .693147180559945)
			+ (x2 * .240226506959101)
			+ (x2 * x * 5.55041086648216E-02)
			+ (x2 * x2 * 9.61812910762848E-03)
			+ (x2 * x2 * x * 1.33335581464284E-03)
			+ (x2 * x2 * x2 * 1.54035303933816E-04)
			));
	}
}

bool port2pitch = true;

uint8_t* Song2Midi(void) {
	unsigned char* ptr;
	unsigned char command;
	channel_t* cp;
	uint8_t buf[8];
	int chan = 0;
	int curWave = -1;
	int curTranspose = -1;
	int track_num = 0;
	unsigned long curVolume_frame = 0;
	float   frequency, temp;
	int		oldpitch = 8192, pitch, diff;

	for (int i = 0; i < 16; i++) {
		cp = &mus_channels[i];

		if (cp->pdata == NULL)
			continue;

		chan = i;
		//
		// avoid percussion channels (default as 9)
		//
		if (chan >= 0x09) {
			chan += 1;
		}

		curWave = -1;
		curTranspose = -1;

		track_num = chan;
		uint8_t last_note = 0;
		while (cp->pdata) {
			
			cp->channel_frame += cp->channel_tempo;
			// process until note or stop for this frame is found
			if (cp->length != 0x7fff)
			{
				while (cp->note_end_frame < cp->channel_frame && cp->pdata != NULL) {
					ptr = cp->pdata;
					while (ptr && (command = *ptr) > 127) {
						// Execute the relevant code for the token.
						//printf("command % 02x\n", command);
						ptr++;
						switch (command)
						{

						case Cstop: { // Fstop - stop data processing
							//printf("Cstop::\t \n");
							ptr = NULL;
							cp->pvolume = NULL;
							cp->ppitchbend = NULL;
							cp->song_addr = NULL;
							cp->IsFX = 0;
							//getch();
							break;
						}

						case Cwave: { // Fwave - set wave number to be played
							unsigned short wave;

							wave = *ptr++;
							if (wave & 0x80)
							{
								wave &= 0x7f;
								wave <<= 8;
								wave |= *ptr++;
							}
							cp->wave = wave;

							//curWave = cp->wave;
							//printf("Cwave::\t wave %d\n", cp->wave);
							break;
						}

						case Cport: { // Fport - enable portamento
							cp->port = *ptr++;
							//printf("Cport::\t port %d\n", cp->port);

							if (port2pitch) { // Portamento pitch
								// set pitch wheel sensitivity
								if (cp->port != 0) {
									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_RP_FINE_COMMAND;
									buf[3] = 0;
									add_event(track_num, cp->note_end_frame, buf);

									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_RP_COARSE_COMMAND;
									buf[3] = 0;
									add_event(track_num, cp->note_end_frame, buf);

									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_RP_PITCH_PARM3;
									buf[3] = 24;
									add_event(track_num, cp->note_end_frame, buf);

									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_RP_PITCH_PARM4;
									buf[3] = 0;
									add_event(track_num, cp->note_end_frame, buf);
								}
							}
							else { // Portamento funciona en versiones recientes de libfluidsynth
								buf[0] = MIDI_CONTROL_CHANGE;
								buf[1] = chan;
								buf[2] = MIDI_PORTAMENTO_COMMAND;
								buf[3] = (cp->port != 0) ? 127 : 0;
								add_event(track_num, cp->note_end_frame, buf);
							}
							break;
						}

						case Cportoff: { // Fportoff - disable portamento
							cp->port = 0;
							//printf("Cportoff::\t port %d\n", cp->port);

							// Portamento funciona en versiones recientes de libfluidsynth
							if (!port2pitch) {
								buf[0] = MIDI_CONTROL_CHANGE;
								buf[1] = chan;
								buf[2] = MIDI_PORTAMENTO_COMMAND;
								buf[3] = cp->port;
								add_event(track_num, cp->note_end_frame, buf);
							}
							break;
						}

						case Cdefa: { // Fdefa - set envelope
							//printf("Cdefa::\t  \n");
							ptr = Fdefa(cp, ptr);
							break;
						}

						case Ctempo: { // Ftempo - set tempo
							// tempo   = bpm
							// fps     = mus_vsyncs_per_second
							// 120 bpm = 96 fps
							// therefore tempo = bmp(required)/120*96/mus_vsyncs_per_second

							channel_t* sp;
							int	i;
							int	temp, temp2;

							//102000
							int tempo = *ptr++;
							//printf("Ctempo::\t  cp->channel_tempo %d\n", tempo);


							temp = (tempo) * 256 * 96 / 120 / 60/*mus_vsyncs_per_second*/;

							//printf("temp %d\n", (temp * 0x80) >> 7);
							temp2 = (temp * cp->temscale) >> 7;
							if (cp->IsFX)
							{
								cp->channel_tempo = temp;
							}
							else
							{
								//for (i = 0, sp = cp; i < max_channels; i++, sp++)
								sp = cp;
								{
									if (sp->song_addr == cp->song_addr)
									{
										sp->channel_tempo_save = temp;
										sp->channel_tempo = temp2;
									}
								}
							}


							int tempo_ = 60000000 / tempo;
							uint8_t* tmp = (uint8_t*)&tempo_;

							buf[0] = MIDI_META_EVENT;
							buf[1] = MIDI_TEMPO_EVENT;
							buf[2] = MIDI_TEMPO_LENGTH;
							buf[3] = tmp[2];
							buf[4] = tmp[1];
							buf[5] = tmp[0];

							add_event(track_num, cp->note_end_frame, buf);

							//printf("Ctempo::\t  cp->channel_tempo_save %d\n", cp->channel_tempo_save);
							//printf("Ctempo::\t  cp->channel_tempo %d\n", cp->channel_tempo);
							break;
						}

						case Cendit: { // Fendit - set endit value

							cp->endit = *ptr++;
							cp->cutoff = 0;

							//printf("Cendit::\t endit %d\n", cp->endit);
							break;
						}

						case Ccutoff: { // Fcutoff - set cutoff value
							short tmp;

							tmp = (*ptr++) << 8;
							tmp |= *ptr++;

							cp->cutoff = tmp;
							cp->endit = 0;

							//printf("Ccutoff::\t cutoff %d\n", cp->cutoff);
							break;
						}

						case Cvibup: { // Fvibup - set positive vibrato
							cp->vib_delay = *ptr++;
							cp->vib_speed = *ptr++;
							cp->vib_amount = ((float)*ptr++) / 50.0;
							//printf("Cvibup::\t vib_delay %d\n", cp->vib_delay);
							//printf("\t vib_speed %d\n", cp->vib_speed);
							//printf("\t vib_amount %f\n", cp->vib_amount);
							break;
						}

						case Cvibdown: { // Fvibdown - set negative vibrato
							cp->vib_delay = *ptr++;
							cp->vib_speed = *ptr++;
							cp->vib_amount = (-((float)*ptr++)) / 50.0;
							//printf("Cvibdown::\t vib_delay %d\n", cp->vib_delay);
							//printf("\t vib_speed %d\n", cp->vib_speed);
							//printf("\t vib_amount %f\n", cp->vib_amount);
							break;
						}

						case Cviboff: { // Fviboff - disable vibrato
							cp->vib_speed = 0;
							cp->vibrato = 0;
							//printf("Cviboff::\t vib_speed = 0 vibrato = 0\n");
							break;
						}

						case Clength: { // Flength - set fixed length

							unsigned char value;

							value = *ptr++;
							if (value < 128)
							{
								cp->fixed_length = value;
							}
							else
							{
								cp->fixed_length = ((int)(value & 0x7f) * 256);
								cp->fixed_length += (int)*ptr++;
							}

							//printf("Clength::\t fixed_length %d\n", cp->fixed_length);

							break;
						}

						case Cignore: { // Fignore - set ignore length flag
							cp->ignore = 1;
							//printf("Cignore::\t ignore = 1\n");
							break;
						}

						case Ctrans: { // Ftranspose - set transpose value
							cp->transpose = *ptr++;
							//printf("Ctrans::\t transpose %d\n", cp->transpose);
							break;
						}

						case Cignoretrans: { // Fignore_trans - set ignore transpose flag
							cp->ignore_transpose = 1;
							//printf("Cignoretrans::\t ignore_transpose = 1\n");
							break;
						}

						case Cdistort: { // distort - set distortion value
							int c;
							float f;

							c = (int)(*ptr++);
							if (c & 0x80)
								c |= 0xffffff00;	// signed chars don't work
							f = (float)(c) / 100.0;

							cp->distort = f;

							//printf("Cdistort::\t distort %f\n", cp->distort);
							break;
						}

						case Cenvelope: { // Fenvelope - set envelope from envelope table
							int tmp;

							tmp = *ptr++;
							if (tmp & 0x80)
							{
								tmp &= 0x7f;
								tmp <<= 8;
								tmp |= *ptr++;
							}
							//printf("Cenvelope::\t env_index %d\n", tmp * 7);
							Fdefa(cp, (unsigned char*)&cp->song_addr->EnvelopeData[tmp * 7]);
							break;
						}

						case Cenvoff: { // Fenvoff - disable envelopes
							cp->env_trigger_off = 1;
							//printf("Cenvoff::\t env_trigger_off = 1\n");
							break;
						}

						case Cenvon: { // Fenvon - enable envelopes
							cp->env_trigger_off = 0;
							//printf("Cenvon::\t env_trigger_off = 0\n");
							break;
						}

						case Ctroff: { // Ftroff - disable triggering
							cp->trigger_off = 1;
							//printf("Ctroff::\t trigger_off = 1\n");
							break;
						}

						case Ctron: { // Ftron - enable triggering
							cp->trigger_off = 0;
							//printf("Ctron::\t trigger_off = 0\n");
							break;
						}

						case Cfor: { // Ffor - start for-next loop (count of 0xFF is infinite)
							int fcount = *ptr++;
							//printf("Cfor::\t fcount %d\n", fcount);
							break;
						}

						case Cnext: { // Fnext - end for-next loop (count of 0xFF is infinite)
							//printf("Cnext\t\n");
							ptr = NULL;
							//getch();
							break;
						}

						case Cwobble: { // Fwobble - define wobble settings
							cp->wobble_amount = *ptr++;
							cp->wobble_on_speed = *ptr++;
							cp->wobble_off_speed = *ptr++;
							//printf("Cwobbleoff::\t wobble_amount %d\n", cp->wobble_amount);
							//printf("\t wobble_on_speed %d\n", cp->wobble_on_speed);
							//printf("\t wobble_off_speed %d\n", cp->wobble_off_speed);
							break;
						}

						case Cwobbleoff: { // Fwobble - disable wobble
							cp->wobble_on_speed = 0;
							//printf("Cwobbleoff::\t wobble_on_speed = 0\n");
							break;
						}

						case Cvelon: { // Fvelon - enable velocity mode
							cp->velocity_on = 1;
							//printf("Cvelon::\t velocity_on = 1\n");
							break;
						}

						case Cveloff: { // Fvelon - disable velocity mode
							cp->velocity_on = 0;
							//printf("Cveloff::\t velocity_on = 0\n");
							break;
						}

						case Cvelocity: { // Fvelon - enable default velocity mode
							int default_velocity = *ptr++;
							cp->velocity_on = 0;
							//printf("Cvelocity::\t default_velocity %d\n", default_velocity);
							break;
						}

						case Cpan: { // Fpan - set pan position
							cp->pan = (*ptr++) / 2;
							//printf("Cpan::\t pan %d\n", cp->pan);

							buf[0] = MIDI_CONTROL_CHANGE;
							buf[1] = chan;
							buf[2] = MIDI_PANPOT_COMMAND;
							buf[3] = cp->pan;
							add_event(track_num, cp->note_end_frame, buf);
							break;
						}

						case Cstereo: { // Fstereo - OUTDATED
							ptr += 2;
							//printf("Cstereo::\t\n");
							break;
						}

						case Cdrums: { // Fdrums - enable drum data
							unsigned long addr;

							addr = (unsigned long)cp->song_addr + (cp->song_addr->DrumData[*ptr++]);
							cp->pdrums = (unsigned char*)addr;
							//printf("Cdrums::\t addr %d\n", addr);
							break;
						}

						case Cdrumsoff: { // Fdrumsoff - disable drum data
							cp->pdrums = NULL;
							//printf("Cdrumsoff::\t\n");
							break;
						}

						case Cprint: { // Fprint - display debugging info (DEBUG MODE ONLY!!!)
							//printf("Cprint::\t\n");
							break;
						}

						case Cgoto: { // Fgoto - sound data ptr relative jump
							//printf("Cgoto::\t\n");
							//getch();
							break;
						}

						case Creverb: { // Freverb - set reverb value
							cp->reverb = *ptr++;
							//printf("Creverb::\t reverb %d\n", cp->reverb);

							buf[0] = MIDI_CONTROL_CHANGE;
							buf[1] = chan;
							buf[2] = MIDI_REVERB_COMMAND;
							buf[3] = cp->reverb;
							add_event(track_num, cp->note_end_frame, buf);
							break;
						}

						case Crandnote: { // FrandNote - set random transpose (within a range)
							// rand_amount,rand_base  -- 20,-3 would give -3 to 16 as the value
							cp->transpose = __MusIntRandom(*ptr++);
							cp->transpose += *ptr++;
							//printf("Crandnote::\t transpose %d\n", cp->transpose);
							break;
						}

						case Crandvolume: { // FrandVolume - set random volume level
							// rand_amount,base
							cp->volume = __MusIntRandom(*ptr++);
							cp->volume += *ptr++;
							//printf("Crandvolume::\t volume %d\n", cp->volume);

							buf[0] = MIDI_CONTROL_CHANGE;
							buf[1] = chan;
							buf[2] = MIDI_VOLUME_COMMAND;
							buf[3] = cp->volume;
							add_event(track_num, cp->note_end_frame, buf);
							break;
						}

						case Crandpan: { // FrandPan - set random pan position
							// rand_amount,base
							cp->pan = __MusIntRandom(*ptr++);
							cp->pan += *ptr++;
							//printf("Crandpan::\t pan %d\n", cp->pan);

							buf[0] = MIDI_CONTROL_CHANGE;
							buf[1] = chan;
							buf[2] = MIDI_PANPOT_COMMAND;
							buf[3] = cp->pan;
							add_event(track_num, cp->note_end_frame, buf);
							break;
						}

						case Cvolume: { // Fvolume - set volume level
							cp->volume = *ptr++;
							//printf("Fvolume::\t volume %d\n", cp->volume);

							buf[0] = MIDI_CONTROL_CHANGE;
							buf[1] = chan;
							buf[2] = MIDI_VOLUME_COMMAND;
							buf[3] = cp->volume;
							add_event(track_num, cp->note_end_frame, buf);
							//getch();
							break;
						}

						case Cstartfx: { // Fstartfx - start sound effect
							int	num;
							num = *ptr++;
							if (num >= 0x80) {
								num = ((num & 0x7f) << 8) + *ptr++;
							}
							//printf("Fstartfx::\t num %d\n", num);
							break;
						}

						default:
							Error("unk (command = % 02x)\n", command);
							break;
						}
					}

					if (cp->wave != curWave) {
						//printf("curWave %d\n", curWave);
						//printf("cp->wave %d\n", cp->wave);
						curEnvelope.wave = cp->wave;
						curEnvelope.transpose = cp->transpose;

						int index = agregarEnvelope(curEnvelope);//add_envelope(curEnvelope);

						buf[0] = MIDI_CONTROL_CHANGE;
						buf[1] = chan;
						buf[2] = MIDI_BANK_SEL_COMMAND;
						buf[3] = (index / 128);
						add_event(track_num, cp->note_end_frame, buf);

						buf[0] = MIDI_PROGRAM_CHANGE;
						buf[1] = chan;
						buf[2] = (index & 127);
						add_event(track_num, cp->note_end_frame, buf);
						curWave = cp->wave;
						curTranspose = cp->transpose;
					}
					else if (curTranspose != cp->transpose) {
						//printf("curTranspose %d\n", curTranspose);
						//printf("cp->transpose %d\n", cp->transpose);
						curEnvelope.wave = cp->wave;
						curEnvelope.transpose = cp->transpose;

						int index = agregarEnvelope(curEnvelope);//add_envelope(curEnvelope);

						buf[0] = MIDI_CONTROL_CHANGE;
						buf[1] = chan;
						buf[2] = MIDI_BANK_SEL_COMMAND;
						buf[3] = (index / 128);
						add_event(track_num, cp->note_end_frame, buf);

						buf[0] = MIDI_PROGRAM_CHANGE;
						buf[1] = chan;
						buf[2] = (index & 127);
						add_event(track_num, cp->note_end_frame, buf);
						curWave = cp->wave;
						curTranspose = cp->transpose;
					}

					//printf("cur ptr %02x\n", *ptr);
					cp->pdata = ptr;

					/* new note */
					if (ptr)
					{
						cp->oldNote = cp->note;
						cp->last_note = cp->port_base;
						cp->note = *(cp->pdata++);
						//printf("cp->note %d\n", cp->note);

						if (cp->velocity_on) {
							cp->velocity = velocity_map[*(cp->pdata++)];
						}
						else {
							cp->velocity = velocity_map[cp->default_velocity];
						}

						if (cp->fixed_length)
						{
							if (!cp->ignore)
								cp->length = cp->fixed_length;
							else
							{
								cp->ignore = 0;
								command = *(cp->pdata++);
								if (command < 128)
									cp->length = command;
								else
									cp->length = ((int)(command & 0x7f) << 8) + *(cp->pdata++);
							}
						}
						else
						{
							command = *(cp->pdata++);
							if (command < 128)
								cp->length = command;
							else
								cp->length = ((int)(command & 0x7f) << 8) + *(cp->pdata++);
						}

						cp->note_start_frame = cp->note_end_frame;
						cp->note_end_frame += cp->length * 256;

						cp->count = 0;
						cp->fcount = 0;
						cp->wobble_count = cp->wobble_off_speed;
						cp->wobble_current = 0;

						if (cp->note != 0) {
							int wave;

							if (cp->pdrums != NULL) {
								cp->wave = (*(cp->pdrums + (cp->note - c1) * 4));
								cp->pan = (*(cp->pdrums + (cp->note - c1) * 4 + 2)) / 2;
								(void)Fdefa(cp, (uint8_t*)&cp->song_addr->EnvelopeData[(*(cp->pdrums + (cp->note - c1) * 4 + 1)) * 7]);
								cp->note = (*(cp->pdrums + (cp->note - c1) * 4 + 3));
							}

							wave = cp->wave >= mus_num_of_samples ? 0 : cp->wave;
							if (cp->trigger_off == 0) {
								cp->playing = 1;
								cp->old_volume = -1; // flag to always rewrite volume on 1st frame
								cp->old_pan = -1;    // flag to always rewrite pan on 1st frame
							}

							//printf("wave %d\n", wave);

							cp->base_note = cp->note + mus_basenote[wave] - 5;

							// Portamento funciona en versiones recientes de libfluidsynth 
							if (!port2pitch) {
								if (cp->port != 0) {
									int mstime = cp->port * 20;
									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_PORTAMENTO_TIME_COARSE_COMMAND;
									buf[3] = (mstime >> 7) & 0x7F;
									add_event(track_num, cp->note_start_frame, buf);

									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_PORTAMENTO_TIME_FINE_COMMAND;
									buf[3] = (mstime & 0x7F);
									add_event(track_num, cp->note_start_frame, buf);

									buf[0] = MIDI_CONTROL_CHANGE;
									buf[1] = chan;
									buf[2] = MIDI_PORTAMENTO_NOTE;
									buf[3] = cp->oldNote;
									add_event(track_num, cp->note_start_frame, buf);
								}
							}

							buf[0] = MIDI_NOTE_ON;
							buf[1] = chan;
							buf[2] = cp->note;
							buf[3] = cp->velocity;
							add_event(track_num, cp->note_start_frame, buf);

							buf[0] = MIDI_NOTE_OFF;
							buf[1] = chan;
							buf[2] = cp->note;
							buf[3] = 0;
							add_event(track_num, cp->note_end_frame, buf);
							//getch();
						}
					}
				}

				// cancel processing if channel stopped
				if (cp->pdata == NULL) {
					break;
				}

				// process volume and pitchbend continous data streams
				if (cp->pvolume != NULL) {
					unsigned char work_vol;
					do {
						curVolume_frame = cp->volume_frame;
						cp->volume_frame += 256;

						cp->cont_vol_repeat_count--;
						if (cp->cont_vol_repeat_count == 0)   // already repeating?
						{
							work_vol = *(cp->pvolume++);
							if (work_vol > 127)		    // does count follow?
							{
								// yes  volume is followed by run length data
								cp->volume = work_vol & 0x7f;
								work_vol = *(cp->pvolume++);
								if (work_vol > 127)
								{
									cp->cont_vol_repeat_count = ((int)(work_vol & 0x7f) * 256);
									cp->cont_vol_repeat_count += (int)*(cp->pvolume++) + 2;
								}
								else
									cp->cont_vol_repeat_count = (int)work_vol + 2;
							}
							else
							{
								cp->volume = work_vol;
								cp->cont_vol_repeat_count = 1;
							}
						}

						if (cp->volume != cp->old_volume) {
							cp->old_volume = cp->volume;
							buf[0] = MIDI_CONTROL_CHANGE;
							buf[1] = chan;
							buf[2] = MIDI_VOLUME_COMMAND;
							buf[3] = cp->volume;
							add_event(track_num, curVolume_frame, buf, true);
						}

					} while ((long)(cp->volume_frame - cp->channel_frame) < 0);
				}

				// base frequency...
				frequency = U8_TO_FLOAT(cp->base_note);

				float freq = -12.0;
				// incorporate portamento...
				if (cp->port != 0)
				{
					float fdiff = frequency - cp->last_note;
					if (fdiff != 0) {
						float lastnote = (-12.56) - fdiff;

						if (cp->count <= cp->port * 2)
						{
							//printf("cp->count %d\n", cp->count);
							//temp = (frequency - cp->last_note) / (float)(cp->port);
							//temp *= cp->fcount;
							//frequency = cp->last_note + temp;

							temp = (freq - lastnote) / (float)(cp->port * 2);
							temp *= cp->fcount;
							freq = lastnote + temp;
						}
					}
				}
				cp->port_base = frequency;

				temp = freq;
				temp = __MusIntPowerOf2(temp * (1.0 / 12.0));
				if (temp < 0) {
					temp = 0;
				}
				if (temp > 2.0) {
					temp = 2.0;
				}

				pitch = static_cast<int>(temp * 16384);

				if (pitch > 16383) {
					pitch = 16383;
				}

				if (pitch < 0) {
					pitch = 0;
				}

				if (pitch != oldpitch) {
					oldpitch = pitch;

					if (port2pitch) {
						buf[0] = MIDI_PITCH_WHEEL;
						buf[1] = chan;
						buf[2] = (pitch & 0x7F);
						buf[3] = ((pitch >> 7) & 0x7F);
						add_event(track_num, cp->channel_frame, buf);
					}
				}

				// increment channel counters
				cp->count = (cp->channel_frame - cp->note_start_frame) >> 8;
				cp->fcount = (float)cp->count;
			}
		}
	}

	return createMidi(curSongName);
}


//**************************************************************
//**************************************************************
//      Sound_CreateWavHeader
//
//  Create a simple WAV header for output
//**************************************************************
//**************************************************************

static void Sound_CreateWavHeader(byte * out, int SAMPLERATE, int nsamp)
{
	uint32_t scratch;

	memcpy(out, "RIFF", 4);
	out += 4;

	scratch = nsamp * 2 + 36;
	memcpy(out, &scratch, 4);
	out += 4;

	memcpy(out, "WAVE", 4);
	out += 4;

	memcpy(out, "fmt ", 4);
	out += 4;

	scratch = 16;		// fmt length
	memcpy(out, &scratch, 4);
	out += 4;

	scratch = 1;		// uncompressed pcm
	memcpy(out, &scratch, 2);
	out += 2;

	scratch = 1;		// mono
	memcpy(out, &scratch, 2);
	out += 2;

	scratch = SAMPLERATE;	// samplerate
	memcpy(out, &scratch, 4);
	out += 4;

	scratch = 2 * SAMPLERATE;	// bytes per second
	memcpy(out, &scratch, 4);
	out += 4;

	scratch = 2;		// blockalign
	memcpy(out, &scratch, 2);
	out += 2;

	scratch = 16;		// bits per sample
	memcpy(out, &scratch, 2);
	out += 2;

	memcpy(out, "data", 4);
	out += 4;

	scratch = nsamp * 2;	// length of pcm data
	memcpy(out, &scratch, 4);
	out += 4;
}

//**************************************************************
//**************************************************************
//      Sound_Decode8
//
//  Decompress current vadpcm sample
//**************************************************************
//**************************************************************

static void Sound_Decode8(byte * in, short *out, int index, short *pred1, short lastsmp[8])
{
	int i;
	short tmp[8];
	long total = 0;
	short sample = 0;
	short *pred2;
	long result;

	memset(out, 0, sizeof(signed short) * 8);

	pred2 = (pred1 + 8);

	for (i = 0; i < 8; i++)
		tmp[i] =
		    itable[(i & 1) ? (*in++ & 0xf) : ((*in >> 4) & 0xf)] <<
		    index;

	for (i = 0; i < 8; i++) {
		total = (pred1[i] * lastsmp[6]);
		total += (pred2[i] * lastsmp[7]);

		if (i > 0) {
			int x;

			for (x = i - 1; x > -1; x--)
				total += (tmp[((i - 1) - x)] * pred2[x]);
		}

		result = ((tmp[i] << 0xb) + total) >> 0xb;

		if (result > 32767)
			sample = 32767;
		else if (result < -32768)
			sample = -32768;
		else
			sample = (signed short)result;

		out[i] = sample;
	}

	// Update the last sample set for subsequent iterations
	memcpy(lastsmp, out, sizeof(signed short) * 8);
}

//**************************************************************
//**************************************************************
//      Sound_DecodeVADPCM
//
//  Decode compressed data
//**************************************************************
//**************************************************************

static uint32_t Sound_DecodeVADPCM(byte * in, short *out, uint32_t len, predictor_t * book)
{
	short lastsmp[8];
	int index;
	int pred;
	byte *pin = in;
	int total = 0;
	int _len = (len / 9) * 9;	//make sure length was actually a multiple of 9
	int samples = 0;
	int x;

	for (x = 0; x < 8; x++)
		lastsmp[x] = 0;

	while (_len) {
		short *pred1;

		index = (*in >> 4) & 0xf;
		pred = (*in & 0xf);
		_len--;

		pred1 = &book->preds[pred * 16];

		Sound_Decode8(++in, out, index, pred1, lastsmp);
		in += 4;
		_len -= 4;

#ifndef USE_SOUNDFONTS
		//
		// cut speed of sound by half
		//
		/*if (!sp->instrument) {
			int j;
			short *rover = out;

			for (j = 0; j < 8; j++) {
				*rover = lastsmp[j];
				rover++;
				*rover = lastsmp[j];
				rover++;
			}

			out += 16;
		} else*/
#endif
			out += 8;

		Sound_Decode8(in, out, index, pred1, lastsmp);
		in += 4;
		_len -= 4;

#ifndef USE_SOUNDFONTS
		//
		// cut speed of sound by half
		//
		/*if (!sp->instrument) {
			int j;
			short *rover = out;

			for (j = 0; j < 8; j++) {
				*rover = lastsmp[j];
				rover++;
				*rover = lastsmp[j];
				rover++;
			}

			out += 16;
		} else*/
#endif
			out += 8;

		samples += 16;
	}

	return samples;
}

//#define USE_LOWPASSFILTER

#ifdef USE_LOWPASSFILTER
#define M_PI 3.14159265358979323846

// Function to apply a low-pass filter to audio data
void filter_audio(std::vector<int16_t>& audio, double cutoffFrequency, double sampleRate) {
	double RC = 1.0 / (2.0 * M_PI * cutoffFrequency);
	double dt = 1.0 / sampleRate;
	double alpha = dt / (RC + dt);

	int16_t prevSample = audio[0];

	for (size_t i = 1; i < audio.size(); ++i) {
		audio[i] = alpha * audio[i] + (1 - alpha) * prevSample;
		prevSample = audio[i];
	}
}

void applyAudioGain(int16_t* audioData, int dataSize, float gain) {
	for (int i = 0; i < dataSize; i++) {
		double scaledSample = static_cast<double>(audioData[i]) * gain;
		if (scaledSample > INT16_MAX) {
			audioData[i] = INT16_MAX;
		}
		else if (scaledSample < INT16_MIN) {
			audioData[i] = INT16_MIN;
		}
		else {
			audioData[i] = static_cast<int16_t>(scaledSample);
		}
	}
}

#endif



//**************************************************************
//**************************************************************
//      Sound_ProcessData
//
//  Setup sound data and convert to 22050hz PCM audio
//**************************************************************
//**************************************************************

static void Sound_ProcessData(wavtable_t * wavtable, int samplerate, predictor_t * pred)
{
	byte *indata;
	uint32_t indatalen;
	short *outdata;
	short *wavdata;
	uint32_t nsamp;

#ifdef USE_SOUNDFONTS
	char samplename[20];
#endif
	uint32_t outsize;

	indatalen = wavtable->size;
	indata = (sfxdata + wavtable->start);

	// not sure if this is the right way to go, it might be possible to simply decode a partial last block
	if (indatalen % 9) {
		while (indatalen % 9)
			indatalen--;
	}

	nsamp = (indatalen / 9) * 16;

#ifndef USE_SOUNDFONTS
	//
	// cut tempo in half for non-instruments.
	// this is a temp workaround until a synth system is implemented
	//
	//if (!in->instrument)
		//nsamp = nsamp * 2;
#endif

	outsize = (nsamp * sizeof(short)) + WAV_HEADER_SIZE;
	outdata = (short *)malloc(outsize);
	wavdata = outdata + (WAV_HEADER_SIZE / 2);

	Sound_CreateWavHeader((byte *) outdata, samplerate, nsamp);
	Sound_DecodeVADPCM(indata, wavdata, indatalen, pred);

#ifdef USE_LOWPASSFILTER
	//filter_audio(audio, cutoffFrequency, wavdata);
	// Read audio data into a vector
	std::vector<int16_t> audio;
	for (int i = 0; i< nsamp; i++) {
		audio.push_back(wavdata[i]);
	}

	// Define the cutoff frequency (adjust this value as needed)
	double cutoffFrequency = 2000.0; // in Hz
	// Apply the low-pass filter to the audio
	filter_audio(audio, cutoffFrequency, samplerate);

	for (int i = 0; i < nsamp; i++) {
		wavdata[i] = audio[i];
	}
	audio.clear();

	//applyAudioGain(wavdata, nsamp, 1.5);
#endif

	

#ifdef USE_SOUNDFONTS
	sprintf(samplename, "SFX_%03d", wavtable->ptrindex);
	SF_AddSampleData(&soundfont, (byte *) wavdata, nsamp * 2, samplename, wavtable->ptrindex);
#else
	char samplename[20];
#endif

	/*sprintf(samplename, "SOUND/SFX_%03d.wav", wavtable->ptrindex);
	FILE* out = fopen(samplename, "wb");
	fwrite((byte*)outdata, sizeof(byte), outsize, out);
	fclose(out);*/

	wavtable->wavsize = outsize;
	wavtabledata[wavtable->ptrindex] = (byte *) outdata;
}

//**************************************************************
//**************************************************************
//      Sound_ProcessPTR
//		Read .ptr bank file
//**************************************************************
//**************************************************************

static void Sound_ProcessPTR(char* fileName) {
	FILE* in;
	char nameHeader[16];
	int32_t pad, data, *offsets;

	in = fopen(fileName, "rb");

	if (!in) {
		Error("Cannot open %s\n", fileName);
	}

	fread(nameHeader, sizeof(char), 16, in);

	if (strcmp(nameHeader, "N64 PtrTables  ")) {
		Error("No es un archivo (.ptr) valido");
	}

	//printf("%s\n", nameHeader);

	fread(&mus_num_of_samples, sizeof(int32_t), 1, in);

	mus_num_of_samples = SwapInt(mus_num_of_samples);

	offsets = (int32_t*)malloc(mus_num_of_samples * sizeof(int32_t));
	for (int i = 0; i < mus_num_of_samples; i++) {
		fread(&data, sizeof(int32_t), 1, in);
		offsets[i] = SwapInt(data);
	}

	//
	// prcess sfx
	//
	sfx = (wavtable_t*)malloc(sizeof(wavtable_t) * mus_num_of_samples);

	for (int i = 0; i < mus_num_of_samples; i++) {
		fseek(in, offsets[i], SEEK_SET);

		fread(&sfx[i].start, sizeof(int32_t), 1, in);
		fread(&sfx[i].size, sizeof(int32_t), 1, in);
		fread(&pad, sizeof(int32_t), 1, in);
		fread(&sfx[i].loopOffset, sizeof(int32_t), 1, in);
		fread(&sfx[i].predOffset, sizeof(int32_t), 1, in);
		
		sfx[i].start = SwapInt(sfx[i].start);
		sfx[i].size = SwapInt(sfx[i].size);
		sfx[i].loopOffset = SwapInt(sfx[i].loopOffset);
		sfx[i].predOffset = SwapInt(sfx[i].predOffset);
		sfx[i].ptrindex = i;
	}


	wavtabledata = (cache*)malloc(sizeof(cache) * mus_num_of_samples);

	for (int i = 0; i < mus_num_of_samples; i++) {
		wavtabledata[i] = NULL;
	}

	//
	// process looptable
	//
	looptable = (looptable_t *)malloc(sizeof(looptable_t) * mus_num_of_samples);
	for (int i = 0; i < mus_num_of_samples; i++) {

		if (sfx[i].loopOffset != 0) {
			fseek(in, sfx[i].loopOffset, SEEK_SET);
			fread(&looptable[i].loopstart, sizeof(int32_t), 1, in);
			fread(&looptable[i].loopend, sizeof(int32_t), 1, in);
			fread(&looptable[i].data, sizeof(int32_t), 10, in);

			looptable[i].loopstart = SwapInt(looptable[i].loopstart) - 16;
			looptable[i].loopend = SwapInt(looptable[i].loopend) - 16;
		}
		else {
			looptable[i].loopstart = 0;
			looptable[i].loopend = 0;
		}
	}

	//
	// process predictors
	//
	predictors = (predictor_t*)malloc(sizeof(predictor_t) * mus_num_of_samples);
	 
	for (int i = 0; i < mus_num_of_samples; i++) {
		fseek(in, sfx[i].predOffset, SEEK_SET);

		fread(&predictors[i].order, sizeof(int32_t), 1, in);
		fread(&predictors[i].npredictors, sizeof(int32_t), 1, in);

		predictors[i].npredictors = SwapInt(predictors[i].npredictors);
		predictors[i].order = SwapInt(predictors[i].order);

		for (int j = 0; j < 64; j++) {
			fread(&predictors[i].preds[j], sizeof(short), 1, in);
			predictors[i].preds[j] = SwapShort(predictors[i].preds[j]);
		}
	}
	

	fclose(in);
}

//**************************************************************
//**************************************************************
//      Sound_ProcessWBK
//		Read .wbk bank file
//**************************************************************
//**************************************************************

static void Sound_ProcessWBK(char* fileName) {
	FILE* in;
	char nameHeader[16];
	int32_t size;

	in = fopen(fileName, "rb");

	if (!in) {
		Error("Cannot open %s\n", fileName);
	}

	fseek(in, 0, SEEK_END);
	size = ftell(in);
	fseek(in, 0, SEEK_SET);

	sfxdata = (byte*) malloc(sizeof(byte) * size);

	fread(sfxdata, sizeof(byte), size, in);

	memcpy(nameHeader, sfxdata, 16);

	if (strcmp(nameHeader, "N64 WaveTables ")) {
		Error("No es un archivo (.ptr) valido");
	}

	//printf("%s\n", nameHeader);

	fclose(in);
}

//**************************************************************
//**************************************************************
//      Sound_ProcessDat
//		Read .dat file
//**************************************************************
//**************************************************************

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

static byte* dataCodePtr;
static int	 dataCodeRegion;

static void Sound_ProcessDat(char* fileName) {
	FILE* in;
	int size;

	in = fopen(fileName, "rb");

	if (!in) {
		Error("Cannot open %s\n", fileName);
	}

	fseek(in, 0, SEEK_END);
	size = ftell(in);
	fseek(in, 0, SEEK_SET);

	dataCodePtr = (byte*)malloc(size * sizeof(byte));

	fread(dataCodePtr, sizeof(char), size, in);

	fclose(in);

	// AC 20 C7 B0 // USA
	// AC 20 DD 70 // EUP
	// AC 20 C2 40 // JAP
	// AC 20 E1 80 // GER
	// AC 20 E3 40 // FRA

	unsigned short *code = (unsigned short*) dataCodePtr;
	//printf("code %x\n", code[3]);

	if (code[3] == 0xB0C7) // Region USA
	{
		dataCodeRegion = REGION_USA;
		//printf("Region USA\n");
	}
	else if (code[3] == 0x70DD) // Region EUP
	{
		dataCodeRegion = REGION_EUP;
		//printf("Region EUP\n");
	}
	else if (code[3] == 0x40C2) // Region JAP
	{
		dataCodeRegion = REGION_JAP;
		//printf("Region JAP\n");
	}
	else if (code[3] == 0x80E1) // Region GER
	{
		dataCodeRegion = REGION_GER;
		//printf("Region GER\n");
	}
	else if (code[3] == 0x40E3) // Region FRA
	{
		dataCodeRegion = REGION_FRA;
		//printf("Region FRA\n");
	}
	else
	{
		Error("Error Region %d\n", code[3]);
		exit(0);
	}
};

//**************************************************************
//**************************************************************
//      Sound_Init
//**************************************************************
//**************************************************************

struct dataOffset {
	int basenote;
	int detune;
	int fxsounds;
	int effects;
};

dataOffset dataOffsets[REGION_MAX] = {
	{0x67B60, 0x67C1C, 0x67F04, 0x68E94}, // Region USA
	{0x67D50, 0x67E0C, 0x680f4, 0x69084}, // Region EUP
	{0x67AD0, 0x67B8C, 0x67E74, 0x68E04}, // Region JAP
	{0x67DE0, 0x67E9C, 0x68184, 0x69114}, // Region GER
	{0x67EB0, 0x67F6C, 0x68254, 0x691E4}  // Region FRA
};

union intFloatUnion {
	int intValue;
	float floatValue;
};

static void Sound_Init(void) {
	intFloatUnion converter;

	//printf("mus_num_of_samples %d\n", mus_num_of_samples);
	mus_basenote = (char*)malloc(mus_num_of_samples * sizeof(char));
	mus_detune = (float*)malloc(mus_num_of_samples * sizeof(float));
	mus_detuneInt = (int*)malloc(mus_num_of_samples * sizeof(int));

	for (int i = 0; i < mus_num_of_samples; i++) {
		mus_basenote[i] = (char)dataCodePtr[i + dataOffsets[dataCodeRegion].basenote];
		//printf("mus_basenote[%d] %d\n", i, mus_basenote[i]);
	}

	int* detune = (int*)&dataCodePtr[dataOffsets[dataCodeRegion].detune];
	for (int i = 0; i < mus_num_of_samples; i++) {
		converter.intValue = SwapInt(detune[i]);
		mus_detune[i] = converter.floatValue;
		mus_detuneInt[i] = (int)((signed char)detune[i]);
		//printf("mus_detune[%d] %f\n", i, mus_detune[i]);
		//printf("mus_detuneInt[%d] %d\n", i, mus_detuneInt[i]);
	}

	int* effects = (int*)&dataCodePtr[dataOffsets[dataCodeRegion].effects];
	int begEffectsOffset = SwapInt(effects[0]);
	for (int i = 0; i < 246; i++) {
		int curEffectsOffset = SwapInt(effects[i]) - begEffectsOffset;
		mus_effects[i] = &dataCodePtr[dataOffsets[dataCodeRegion].fxsounds + curEffectsOffset];
		//printf("mus_effects[%d] %x\n", i, *mus_effects[i]);
	}

	for (int i = 0; i < MAXCHANNELS; i++) {
		channel_t* cp = &mus_channels[i];
		__MusIntInitialiseChannel(cp);
	}
}

//**************************************************************
//**************************************************************
//      Sound_Setup
//**************************************************************
//**************************************************************

void Sound_Setup(void)
{
	//printf("Sound_Setup\n");
	Sound_ProcessPTR("HEXEN64.PTR");
	Sound_ProcessWBK("HEXEN64.WBK");
	Sound_ProcessDat("HEXEN64.DAT");
	Sound_Init();
}

//**************************************************************
//**************************************************************
//      Sound_Setup
//**************************************************************
//**************************************************************

void Sound_FreePtrs(void)
{
	free(mus_basenote);
	free(mus_detune);
	free(mus_detuneInt);
	free(dataCodePtr);
	// Liberar la memoria del arreglo dinámico al final del programa
	if (g_envelopes != nullptr) {
		delete[] g_envelopes;
	}
}

//**************************************************************
//**************************************************************
//      Sound_WriteSoundFont
//**************************************************************
//**************************************************************

void Sound_WriteSoundFont(void) {
#ifdef USE_SOUNDFONTS
	SF_Setup();

	getcoord.Y++;
	for (int i = 0; i < mus_num_of_samples; i++) {
		wavtable_t* wavtable;
		int j;

		wavtable = &sfx[i];

		if (wavtabledata[wavtable->ptrindex])
			continue;

		PrintfPorcentaje(i, mus_num_of_samples-1, true, getcoord.Y, "Decompressing audio...\t\t");
		Sound_ProcessData(wavtable, 22050, &predictors[wavtable->ptrindex]);
	}

	SF_AddSample(&soundfont.samples, "EOS", 0, 0, 0);
	SF_SetupModulators();
	SF_CreatePresets(sfx);
	SF_FinalizeChunkSizes();

	// Write out the soundfont file
	printf("Writing Soundfont File...\n");
	SF_WriteSoundFont();
#endif
}
