#include <stdio.h>
#include <string.h>

typedef enum {
    false,
    true
} bool;

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

    return true;
}

int main()
{
    FILE* fp;
    struct Mod mod;

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
        fclose(fp);
    }
}
