#ifndef _MOD_H_
#define _MOD_H_

#include <stdio.h>

enum ModEffect {
    MOD_EFFECT_ARPEGGIO,
    MOD_EFFECT_SLIDE_UP,
    MOD_EFFECT_SLIDE_DOWN,
    MOD_EFFECT_PORTAMENTO,
    MOD_EFFECT_VIBRATO,
    MOD_EFFECT_PORTAMENTO_AND_VOLUME_SLIDE,
    MOD_EFFECT_VIBRATO_AND_VOLUME_SLIDE,
    MOD_EFFECT_TREMOLO,
    MOD_EFFECT_SET_PANNING,
    MOD_EFFECT_SET_SAMPLE_OFFSET,
    MOD_EFFECT_VOLUME_SLIDE,
    MOD_EFFECT_POSITION_JUMP,
    MOD_EFFECT_SET_VOLUME,
    MOD_EFFECT_PATTERN_BREAK,
    MOD_EFFECT_COMPLEX,
    MOD_EFFECT_SET_SPEED
};

struct ModSample {
    char name[22];
    int length;
    int loop_point;
    int loop_length;
    char volume;
    char fine_tuning;
    int is_looping;
    char* data;
};

struct ModPatternEntry {
    int note_period;
    int instrument;
    enum ModEffect effect;
    unsigned char effect_data;
};

struct ModPattern {
    struct ModPatternEntry row[64][4];
};

struct Mod {
    char song_title[20];
    struct ModSample samples[31];
    char song_length;
    char pattern_count;
    char pattern_table[128];
    struct ModPattern pattern[128];
};

extern int load_mod(struct Mod* mod, FILE* fp);
extern int amiga_period_table[];
#endif
