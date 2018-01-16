#include <stdio.h>
#include <string.h>

typedef enum {
    false,
    true
} bool;

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
    int loop_start;
    int loop_end;
    char volume;
    char fine_tuning;
    bool is_looping;
    unsigned char* data;
};

struct Mod {
    char song_title[20];
    struct ModSample samples[31];
    char song_length;
    char pattern_count;
    char pattern_table[128];
};

struct ModPatternEntry {
    int note_index;
    int instrument;
    enum ModEffect effect;
    unsigned char effect_data;
};

static char* note_names[] = {
    "C-",
    "C#",
    "D-",
    "D#",
    "E-",
    "F-",
    "F#",
    "G-",
    "G#",
    "A-",
    "A#",
    "B-"
};

int amiga_period_table[] = {
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906,
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
    107, 101, 95, 90, 85, 80, 75, 71, 67, 63, 60, 56
};

int fread_big_endian_word(FILE* fp)
{
    unsigned char hi_byte = 0, lo_byte = 0;
    fread(&hi_byte, sizeof(char), 1, fp);
    fread(&lo_byte, sizeof(char), 1, fp);
    return hi_byte << 8 | lo_byte;
}

void read_sample_record(struct ModSample* rec, FILE* fp)
{
    int half_loop_length;
    fread(rec->name, sizeof(char), 22, fp);
    rec->length = fread_big_endian_word(fp) * 2;
    fread(&rec->fine_tuning, sizeof(char), 1, fp);
    if ((rec->fine_tuning &= 0x0F) & 0x08)
        rec->fine_tuning |= 0xF0; /* Manual sign extension */
    fread(&rec->volume, sizeof(char), 1, fp);

    rec->loop_start = fread_big_endian_word(fp) * 2;
    half_loop_length = fread_big_endian_word(fp) * 2;

    if (half_loop_length > 1) {
        rec->loop_end = rec->loop_start + half_loop_length * 2;
        rec->is_looping = true;
    }
}

int index_of_period(int period) {
    int i;
    for (i = 0; i < 12 * 5; i++)
        if (period == amiga_period_table[i])
            return i;
    return -1;
}

void read_pattern(FILE* fp) {
    struct ModPatternEntry entry;
    unsigned char channel_data[4];
    int period;

    fread(channel_data, sizeof(char), 4, fp);

    period = (channel_data[0] & 0x0f) << 8 | channel_data[1];

    if (period) {
        entry.note_index = index_of_period(period);
        if (entry.note_index == -1)
            fprintf(stderr, "WARNING: Period %d not found in table\n", period);
    }
    else
        entry.note_index = 0;

    entry.instrument = (channel_data[0] & 0xf0) | (channel_data[2] >> 4);
    entry.effect = channel_data[2] & 0x0f;
    entry.effect_data = channel_data[3];

    if (entry.note_index)
        printf("%s%d %02d %x%02X",
            note_names[entry.note_index % 12],
            entry.note_index / 12,
            entry.instrument,
            entry.effect, entry.effect_data);
    else
        printf("... %02d %x%02X",
            entry.instrument,
            entry.effect, entry.effect_data);

}

int load_mod(struct Mod* mod, FILE* fp)
{
    char signature[5];
    int i;

    fseek(fp, 0x438, SEEK_SET);
    fread(signature, sizeof(char), 4, fp);
    signature[4] = '\0';

    /* Check for "M.K." signature */
    if (strncmp(signature, "M.K.", 4) != 0)
        return false;

    rewind(fp);
    fread(mod->song_title, sizeof(char), 20, fp);

    /* There are always 31 sample records. Empty samples are indicated by lengths of 0 */
    for (i = 0; i < 31; i++)
        read_sample_record(&mod->samples[i], fp);

    fread(&mod->song_length, sizeof(char), 1, fp);
    /* Skip a (typically) unused byte */
    fseek(fp, 1, SEEK_CUR);
    fread(mod->pattern_table, sizeof(char), 128, fp);

    /* The number of patterns is equal to the highest value in the pattern table */
    for (i = 0, mod->pattern_count = 0; i < mod->song_length; i++)
        if (mod->pattern_table[i] > mod->pattern_count)
            mod->pattern_count = mod->pattern_table[i];

    /* Skip already loaded signature */
    fseek(fp, 4, SEEK_CUR);

    /* Begins pattern data. 4 bytes * #channels * 64 rows*/
    return true;
}

int main()
{
    FILE* fp;
    struct Mod mod;
    int i, j;

    if ((fp = fopen("/Users/pironvila/Downloads/soul-o-matic.mod", "rb"))) {

        if (load_mod(&mod, fp)) {
            int i;
            char* prefix = "";

            printf("Song Title: %s\n", mod.song_title);
            printf("Song Length: %d\n", mod.song_length);
            printf("Pattern Count: %d\n", mod.pattern_count);

            printf("Pattern Order: ");
            for (i = 0; i < mod.song_length; i++) {
                printf("%s%d", prefix, mod.pattern_table[i]);
                prefix = ", ";
            }
            printf("\n");

            for (i = 0; i < 31; i++)
                printf("%02d: %-20s (Len: %d, Beg: %d, End: %d)\n",
                    i, mod.samples[i].name, mod.samples[i].length,
                    mod.samples[i].loop_start, mod.samples[i].loop_end);
        }
        printf("PATTERN START: %04X\n", (int)ftell(fp));

        for (i = 0; i < 64; i++) {
            printf("%02d: ", i);
            for (j = 0; j < 4; j++) {
                read_pattern(fp);
                printf(" | ");
            }
            printf("\n");
        }

        fclose(fp);
    }
}
