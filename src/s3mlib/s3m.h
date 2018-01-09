#ifndef _S3M_H_
#define _S3M_H_

#pragma pack(push, 1)
/*
                                S3M Module header
          0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
        +---------------------------------------------------------------+
  0000: | Song name, max 28 chars (end with NUL (0))                    |
        |---------------------------------------------------------------|
  0010: |                                               |1Ah|Typ| x | x |
        |---------------------------------------------------------------|
  0020: |OrdNum |InsNum |PatNum | Flags | Cwt/v | Ffi   |'S'|'C'|'R'|'M'|
        |---------------------------------------------------------------|
  0030: |g.v|i.s|i.t|m.v|u.c|d.p| x | x | x | x | x | x | x | x |Special|
        |---------------------------------------------------------------|
  0040: |Channel settings for 32 channels, 255=unused,+128=disabled     |
        |---------------------------------------------------------------|
  0050: |                                                               |
        |---------------------------------------------------------------|
  0060: |Orders; length=OrdNum                                          |
        |---------------------------------------------------------------|
  xxx1: |Parapointers to instruments; length=InsNum*2                   |
        |---------------------------------------------------------------|
  xxx2: |Parapointers to patterns; length=PatNum*2                      |
        |---------------------------------------------------------------|
  xxx3: |Channel default pan positions                                  |
        +---------------------------------------------------------------+
        xxx1=60h+orders
        xxx2=60h+orders+instruments*2
        xxx3=60h+orders+instruments*2+patterns*2

        Parapointers to file offset Y is (Y-Offset of file header)/16.
        You could think of parapointers as segments relative to the
        start of the S3M file.*/

struct S3MModuleHeader {
    char song_name[28];
    unsigned char gap_0; /* Must be 0x1A */
    unsigned char type; /* Must be 16 */
    char gap_1[2]; /* 0x1E-0x1F */
    short order_count;
    short instrument_count;
    short pattern_count;
    short flags;
    short created_with; /* 0x1300=ST3.00, 0x1301=ST3.01, 0x1303=ST3.03, 0x1320=ST3.20 */
    short file_format_info;
    char SCRM[4]; /* Must be string "SCRM" */
    unsigned char global_volume;
    unsigned char initial_speed;
    unsigned char initial_tempo;
    unsigned char master_volume;
    unsigned char ultra_click_removal;
    unsigned char default_pan;
    char gap_2[8]; /* 0x36-0x0x3D */
    short special; /* pointer to special custom data (not used by ST3.01) */
    unsigned char channel_settings[32]; /* 255=unused, +128=disabled */
};

/*
          0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
        +---------------------------------------------------------------+
  0000: |[T]| Dos filename (12345678.ABC)                   |    MemSeg |
        ----------------------------------------------------------------|
  0010: |Length |HI:leng|LoopBeg|HI:LBeg|LoopEnd|HI:Lend|Vol| x |[P]|[F]|
        ----------------------------------------------------------------|
  0020: |C2Spd  |HI:C2sp| x | x | x | x |Int:Gp |Int:512|Int:lastused   |
        ----------------------------------------------------------------|
  0030: | Sample name, 28 characters max... (incl. NUL)                 |
        ----------------------------------------------------------------|
  0040: | ...sample name...                             |'S'|'C'|'R'|'S'|
        +---------------------------------------------------------------+
  xxxx:	sampledata
*/
struct S3MSampleHeader {
    unsigned char type;
    char dos_filename[12];
    char null_terminator;
    short sample_data_parapointer;
    int length;
    int loop_begin;
    int loop_end;
    unsigned char default_volume;
    char gap_0;
    unsigned char packing;
    unsigned char flags;
    int c2_speed;
    char gap_1[4];
    short gravis_memory;
    short sb_loop_flags;
    int last_used_position;
    char name[28];
    char SCRS[4];
};

#pragma pack(pop)

struct S3MPackedPattern {
    int length;
    unsigned char* data;
};

struct S3MSampleInstrument {
    struct S3MSampleHeader* header;
    unsigned char* sampledata;
};

struct S3MFile {
    unsigned char* file_data;
    struct S3MModuleHeader* header;
    unsigned char* orders;
    struct S3MSampleInstrument instruments[99];
    struct S3MPackedPattern packed_patterns[100];
    unsigned char* default_channel_pan;
};

struct S3MPatternEntry {
    unsigned char note; /* FF=empty */
    unsigned char inst; /* 0=.. */
    unsigned char vol; /* 255=.. */
    unsigned char command; /* 255=.. */
    unsigned char cominfo; /* 00=Continue Last */
};

struct S3MPattern {
    struct S3MPatternEntry row[64][32];
};

struct S3MChannel {
    struct S3MSampleInstrument* instrument;
    int note_on;
    int period;
    int volume;
    int panning; /* 0 (left) - F (right) */
    int current_effect;
    struct {
        int volume_slide_speed;
        int is_fine_slide;
    } effects;
};

struct S3MSampleStream {
    struct S3MSampleInstrument* sample;
    struct S3MChannel* channel;
    float sample_index;
    float sample_step;
};

struct S3MPlayerContext {
    struct S3MFile* file;
    int song_tempo;
    int song_speed;
    int tick_counter;
    int current_row;
    int samples_per_tick;
    int samples_until_next_tick;
    int sample_rate;

    struct S3MPattern* patterns;
    int current_order;
    int current_pattern;

    struct S3MChannel channel[32];
    struct S3MSampleStream sample_stream[16];
};

extern int s3m_load(struct S3MFile*, const char*);
extern void s3m_render_audio(float*, int, struct S3MPlayerContext*);
extern void s3m_player_init(struct S3MPlayerContext*, struct S3MFile*, int);

#endif
