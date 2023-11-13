#ifndef _SOUND_H_
#define _SOUND_H_

#include <stdint.h>
#include <cstring>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <string>
#include <conio.h>
#include <windows.h>

#define USE_SOUNDFONTS

typedef unsigned char byte;
typedef byte* cache;

extern byte *sfxdata;

typedef struct {
	uint32_t start;		// start of rom offset
	uint32_t size;		// size of sfx
	uint32_t wavsize;	// suppose to be pad but used as temp size for wavdata
	uint32_t loopOffset;// correction pitch
	uint32_t predOffset;// index id for loop table
	uint32_t ptrindex;	// suppose to be pad2 but used as pointer index by wadgen
} wavtable_t;

extern wavtable_t *sfx;
extern cache *wavtabledata;

typedef struct {
	uint32_t loopstart;
	uint32_t loopend;
	uint32_t data[10];		// its nothing but pure initialized garbage in rom but changed/set in game
} looptable_t;

extern looptable_t *looptable;

typedef struct {
	uint32_t order;			// order id
	uint32_t npredictors;	// number of predictors
	short preds[64];		// predictor array
} predictor_t;

extern const char* songlumpnames[];
void Sound_Setup(void);
void Sound_FreePtrs(void);
void Sound_WriteSoundFont(void);

void MusStartSong(uint8_t* data, const char* fileName);
void MusStartEffect(int number);
uint8_t* Song2Midi(void);

extern byte* mus_effects[246];

#define U8_TO_FLOAT(c) ((c)&128) ? -(256-(c)) : (c)

#define MAXCHANNELS 16
#define MAXPOINTERS 2
#define MAXDATAS 4

/* standard Music Tools binary song header used in Hexen 64 */
/* possibly from a version prior to 1.0 of Music Tools */
typedef struct
{
	char* ChannelData[MAXCHANNELS];
	char* VolumeData[MAXCHANNELS];
	//char* PitchBendData[MAXCHANNELS];
	char* EnvelopeData;
	unsigned long* DrumData;
} song_t;

typedef struct
{
	int ChannelDataOffset;
	int VolumeDataOffset;
	int EnvelopeDataOffset;
	int DrumDataOffset;
} song_Header_t;

/* note values */
#define g1	19
#define f1	17
#define ds1	15
#define as1	22
#define c2	24
#define a1	21
#define g3	43
#define d3	38
#define g2	31
#define d2	26
#define f3	41
#define f2	29
#define g0	7
#define d1	14
#define ds3	39
#define ds2	27
#define as5	70
#define g5	67
#define f5	65
#define d6	74
#define ds6	75
#define f6	77
#define g4	55
#define d5	62
#define as4	58
#define f4	53
#define ds5	63
#define ds4	51
#define g6	79
#define d0	2
#define c1	12
#define cs0	1
#define c6	72
#define as2	34
#define c3	36
#define as3	46
#define c4	48
#define d4	50
#define c5	60
#define a4	57
#define cs1	13
#define ds0	3
#define e0	4
#define f0	5
#define fs0	6
#define gs0	8
#define a0	9
#define as0	10
#define b0	11
#define e1	16
#define fs1	18
#define gs1	20
#define b1	23
#define cs2	25
#define e2	28
#define fs2	30
#define gs2	32
#define a2	33
#define b2	35
#define cs3	37
#define e3	40
#define fs3	42
#define gs3	44
#define a3	45
#define b3	47
#define cs4	49
#define e4	52
#define fs4	54
#define gs4	56
#define b4	59
#define cs5	61
#define e5	64
#define fs5	66
#define gs5	68
#define a5	69
#define b5	71
#define cs6	73
#define e6	76
#define fs6	78
#define gs6	80
#define a6	81
#define as6	82
#define b6	83
#define c7	84
#define cs7	85
#define d7	86
#define ds7	87
#define e7	88
#define f7	89
#define fs7	90
#define g7	91
#define gs7	92
#define a7	93
#define as7	94

/* commands */
#define Cstop		0x80
#define Cwave		0x81	/*0x89*/
#define Cport		0x82	/*0x90*/
#define	Cportoff	0x83
#define Cdefa		0x84	/*0xa2*/
#define Ctempo		0x85	/*0xb6*/
#define Ccutoff		0x86
#define Cendit		0x87
#define Cvibup		0x88
#define Cvibdown	0x89
#define	Cviboff		0x8a
#define	Clength		0x8b
#define	Cignore		0x8c
#define	Ctrans		0x8d
#define	Cignoretrans	0x8e
#define	Cdistort	0x8f
#define Cenvelope		0x90
#define Cenvoff		0x91
#define Cenvon		0x92
#define Ctroff		0x93
#define	Ctron		0x94
#define Cfor		0x95
#define	Cnext		0x96
#define	Cwobble		0x97
#define Cwobbleoff	0x98
#define Cvelon		0x99
#define	Cveloff		0x9a
#define Cvelocity	0x9b
#define Cpan		0x9c
#define Cstereo		0x9d
#define	Cdrums		0x9e
#define	Cdrumsoff	0x9f
#define	Cprint		0xa0
#define	Cgoto		0xa1
#define	Creverb		0xa2
#define	Crandnote	0xa3
#define	Crandvolume	0xa4
#define	Crandpan	0xa5
#define	Cvolume		0xa6
#define	Cstartfx       	0xa7

/* maximum depth of for-next loops */
#define FORNEXT_DEPTH	5

/* music player channel structure */
typedef struct
{
	/* 32bit values... */
	unsigned long		channel_frame;
	unsigned long		volume_frame;
	unsigned long		pitchbend_frame;
	unsigned long		note_end_frame;
	unsigned long		note_start_frame;
	long			handle;
	int     		stopping;
	int    		stopping_speed;
	int  			priority;
	float		        old_frequency;
	float			f_length;
	float			distort;
	float			env_attack_calc;
	float			env_decay_calc;
	float			env_release_calc;
	float			last_note;
	float			fcount;
	float			vib_amount;
	float			vibrato;
	float			port_base;
	float			freqoffset;
	// pointers (also 32bit)...
	song_t* song_addr;
	unsigned char* pdata;
	unsigned char* pbase;
	unsigned char* pvolume;
	unsigned char* pvolumebase;
	unsigned char* ppitchbend;
	unsigned char* ppitchbendbase;
	unsigned char* pdrums;
	unsigned char* for_stack[FORNEXT_DEPTH];
	unsigned char* for_stackvol[FORNEXT_DEPTH];
	unsigned char* for_stackpb[FORNEXT_DEPTH];
	// 16bit values...
	short			volscale;
	short			panscale;
	short			temscale;
	unsigned short	old_volume;
	unsigned short	channel_tempo;
	unsigned short	channel_tempo_save;
	unsigned short	length;
	unsigned short	IsFX;
	unsigned short	fixed_length;
	unsigned short	cutoff;
	unsigned short	endit;
	unsigned short	count;
	unsigned short	cont_vol_repeat_count;
	unsigned short	cont_pb_repeat_count;
	unsigned short       	wave;
	unsigned short	for_vol_count[FORNEXT_DEPTH];
	unsigned short	for_pb_count[FORNEXT_DEPTH];
	// 8bit values...
	unsigned char		old_reverb;
	unsigned char		old_pan;
	unsigned char		playing;
	unsigned char		trigger_off;
	unsigned char		ignore;
	unsigned char		ignore_transpose;
	unsigned char		env_trigger_off;
	unsigned char		vib_speed;/* zero if not active*/
	unsigned char		port;
	unsigned char		wobble_on_speed;
	unsigned char		for_stack_count;
	unsigned char		velocity_on;
	unsigned char		default_velocity;
	unsigned char		volume;
	unsigned char		pitchbend;
	unsigned char		bendrange;
	unsigned char		pan;
	unsigned char		reverb;			// wet/dry mix for alSynSetMix()
	unsigned char		env_speed;
	unsigned char		env_init_vol;
	unsigned char		env_max_vol;
	unsigned char		env_sustain_vol;
	unsigned char		env_phase;
	unsigned char		env_current;
	unsigned char		env_count;
	unsigned char		env_attack_speed;
	unsigned char		env_decay_speed;
	unsigned char		env_release_speed;
	unsigned char		oldNote;
	unsigned char		note;
	unsigned char		base_note;
	unsigned long		release_frame;
	unsigned char		release_start_vol;
	unsigned char		vib_delay;
	unsigned char		wobble_off_speed;
	unsigned char		wobble_count;
	unsigned char		velocity;
	unsigned char		for_count[FORNEXT_DEPTH];
	unsigned char		for_volume[FORNEXT_DEPTH];
	unsigned char		for_pitchbend[FORNEXT_DEPTH];
	signed char		transpose;
	signed char		wobble_amount;
	signed char		wobble_current;
} channel_t;

typedef struct {
	uint32_t deltaTime;
	uint8_t buf[8];
} midiEvent_t;

typedef struct {
	std::vector<midiEvent_t> midi_events[16];
} vec_miditrack_t;

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

typedef PACK(struct {
	char header[4];
	int length;
	uint8_t* data;
}) miditrack_t;

typedef PACK(struct {
	char header[4];
	int chunksize;
	short type;
	uint16_t ntracks;
	uint16_t delta;
	uint32_t size;
	miditrack_t* tracks;
}) midiheader_t;

#define MIDI_HEADER_BYTES          14
#define MIDI_HEADER_LENGTH         6
#define MIDI_TRACK_HEADER_BYTES    8
#define MIDI_TYPE_0                0
#define MIDI_TYPE_1                1

#define MIDI_VAR_LENGTH_CONT_MASK  0x80
#define MIDI_VAR_LENGTH_CONT(v)    ((v & MIDI_VAR_LENGTH_CONT_MASK) == \
                                    MIDI_VAR_LENGTH_CONT_MASK)

#define MIDI_VAR_LENGTH_MAX        4
#define MIDI_VAR_LENGTH_BITS_MASK  0x7F
#define MIDI_VAR_LENGTH_BITS_SHIFT 0x07
#define MIDI_VAR_LENGTH_BITS(v)    (v & MIDI_VAR_LENGTH_BITS_MASK)
#define MIDI_VAR_LENGTH_CALC(m,v)  ((m << MIDI_VAR_LENGTH_BITS_SHIFT) | \
                                    MIDI_VAR_LENGTH_BITS(v))

#define MIDI_STATUS_BIT            0x80
#define MIDI_STATUS_BYTE(v)        ((v & MIDI_STATUS_BIT) == MIDI_STATUS_BIT)

#define MIDI_STATUS_NIBBLE         0xF0
#define MIDI_CHANNEL_NIBBLE        0x0F
#define MIDI_NOTE_OFF              0x80
#define MIDI_NOTE_ON               0x90
#define MIDI_AFTER_TOUCH           0xA0
#define MIDI_CONTROL_CHANGE        0xB0
#define MIDI_PROGRAM_CHANGE        0xC0
#define MIDI_CHANNEL_PRESSURE      0xD0
#define MIDI_PITCH_WHEEL           0xE0
#define MIDI_SYSTEM_MESSAGE        0xF0
#define MIDI_RESET_COMMAND         0xFF

#define MIDI_ALL_CHANNELS          0x10
#define MIDI_BANK_SEL_COMMAND      0x00
#define MIDI_FINE_BANK_SEL_COMMAND 0x20
#define MIDI_MODULATION_COMMAND    0x01
#define MIDI_PORTAMENTO_TIME_COARSE_COMMAND 0x05
#define MIDI_PORTAMENTO_TIME_FINE_COMMAND	0x25
#define MIDI_VOLUME_COMMAND        0x07
#define MIDI_PANPOT_COMMAND        0x0A
#define MIDI_EXPRESSION_COMMAND    0x0B
#define MIDI_PORTAMENTO_COMMAND    0x41
#define MIDI_PORTAMENTO_NOTE	   0x54
#define MIDI_REVERB_COMMAND        0x5B
#define MIDI_RP_FINE_COMMAND       0x64
#define MIDI_RP_COARSE_COMMAND     0x65
#define MIDI_RP_RESET_PARM         0x00
#define MIDI_STOP_COMMAND          0x78
#define MIDI_ALL_CONTROLLERS_RESET 0x79
#define MIDI_ALL_NOTE_OFF_COMMAND  0x7B

#define MIDI_RP_TUNE_PARM1         0x02
#define MIDI_RP_TUNE_PARM2         0x00
#define MIDI_RP_TUNE_PARM3         0x06

#define MIDI_RP_PITCH_PARM1        0x00
#define MIDI_RP_PITCH_PARM2        0x00
#define MIDI_RP_PITCH_PARM3        0x06
#define MIDI_RP_PITCH_PARM4        0x26
#define MIDI_RP_PITCH_PARM5        0x00

#define MIDI_SYS_EXCLUSIVE         0xF0
#define MIDI_END_SYS_EXCLUSIVE     0xF7

#define MIDI_META_EVENT            0xFF
#define MIDI_END_OF_TRACK          0x2F
#define MIDI_NOP_EVENT             0x00
#define MIDI_TEMPO_EVENT           0x51
#define MIDI_TEMPO_LENGTH          0x03

typedef struct
{
	unsigned short		wave;
	int					transpose;
	unsigned char		env_speed;
	unsigned char		env_init_vol;
	unsigned char		env_max_vol;
	unsigned char		env_sustain_vol;
	unsigned char		env_attack_speed;
	unsigned char		env_decay_speed;
	unsigned char		env_release_speed;
} envelope_t;

extern char* mus_basenote;
extern float* mus_detune;
extern int* mus_detuneInt;

extern envelope_t* g_envelopes;
extern int cantidadEnvelopes;

#endif