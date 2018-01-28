#include "mod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    fread(rec->name, sizeof(char), 22, fp);
    rec->length = fread_big_endian_word(fp) * 2;
    fread(&rec->fine_tuning, sizeof(char), 1, fp);
    if ((rec->fine_tuning &= 0x0F) & 0x08)
        rec->fine_tuning |= 0xF0; /* Manual sign extension */
    fread(&rec->volume, sizeof(char), 1, fp);

    rec->loop_point = fread_big_endian_word(fp) * 2;
    rec->loop_length = fread_big_endian_word(fp) * 2;

    if (rec->loop_length > 2)
        rec->is_looping = 1;
}

int index_of_period(int period)
{
    int i;
    for (i = 0; i < 12 * 5; i++)
        if (period == amiga_period_table[i])
            return i;
    return -1;
}

void read_pattern_entry(struct ModPatternEntry* entry, FILE* fp)
{
    unsigned char channel_data[4];

    fread(channel_data, sizeof(char), 4, fp);
    entry->note_period = (channel_data[0] & 0x0f) << 8 | channel_data[1];
    entry->instrument = (channel_data[0] & 0xf0) | (channel_data[2] >> 4);
    entry->effect = channel_data[2] & 0x0f;
    entry->effect_data = channel_data[3];
}

void read_pattern(struct ModPattern* pattern, int num_channels, FILE* fp)
{
    int i, j;
    for (i = 0; i < 64; i++)
        for (j = 0; j < num_channels; j++)
            read_pattern_entry(&pattern->row[i][j], fp);
}

int load_mod(struct Mod* mod, FILE* fp)
{
    char signature[5];
    int i;

    fseek(fp, 0x438, SEEK_SET);
    fread(signature, sizeof(char), 4, fp);
    signature[4] = '\0';

    /* Check for "M.K." signature */
    if (strncmp(signature, "M.K.", 4) == 0)
        mod->num_channels = 4;
    else if (strncmp(signature, "8CHN", 4) == 0)
        mod->num_channels = 8;
    else
        return 0;

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
    mod->pattern_count++;

    /* Skip already loaded signature */
    fseek(fp, 4, SEEK_CUR);

    /* Begins pattern data. 4 bytes * #channels * 64 rows*/
    for (i = 0; i < mod->pattern_count; i++)
        read_pattern(&mod->pattern[i], mod->num_channels, fp);

    printf("Start of Sample Data %d", (int)ftell(fp));

    /* Sample data follows pattern data */
    for (i = 0; i < 31; i++) {
        if (mod->samples[i].length) {
            printf("Reading Sample #%d\n", i);
            printf("Length %d, Loop: %d Loop_Len: %d\n", mod->samples[i].length, mod->samples[i].loop_point, mod->samples[i].loop_length);
            mod->samples[i].data = malloc(sizeof(char) * mod->samples[i].length);
            fread(mod->samples[i].data, sizeof(char), mod->samples[i].length, fp);
        }
    }

    return 1;
}
