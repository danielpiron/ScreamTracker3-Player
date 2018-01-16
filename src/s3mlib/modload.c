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
    unsigned char *data;
};

struct Mod {
    char song_title[20];
    struct ModSample samples[31];
    char song_length;
    char pattern_count;
    char pattern_table[128];
};

int fread_big_endian_word(FILE *fp) {
    unsigned char hi_byte = 0, lo_byte = 0;
    fread(&hi_byte, sizeof(char), 1, fp);
    fread(&lo_byte, sizeof(char), 1, fp);
    return hi_byte << 8 | lo_byte;
}

void read_sample_record(struct ModSample* rec, FILE* fp) {
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
        rec->loop_end = rec->loop_start + half_loop_length  * 2;
        rec->is_looping = true;
    }
}

void print_sample(struct ModSample* sample) {
    printf("\tName: %s\n", sample->name);
    printf("\tLength: %d\n", sample->length);
    printf("\tVolume: %d\n", sample->volume);
    printf("\tFine Tuning: %d\n" , sample->fine_tuning);
    printf("\tLooping Enabled: %s\n", sample->is_looping ? "YES" : "NO");
    if (sample->is_looping) {
        printf("\tLoop Start: %d\n", sample->loop_start);
        printf("\tLoop End: %d\n", sample->loop_end);
    }
}
        
int main()
{
    int i;
    FILE* fp;
    struct Mod mod;
    char signature[5];

    if ((fp = fopen("/Users/pironvila/Downloads/soul-o-matic.mod", "rb"))) {

        fseek(fp, 0x438, SEEK_SET);
        fread(signature, sizeof(char), 4, fp);
        signature[4] = '\0';

        printf("Signature %s\n", signature);
        if (strncmp(signature, "M.K.", 4) == 0) {
            printf("MOD file signature found\n");
        }

        rewind(fp);
        fread(mod.song_title, sizeof(char), 20, fp);

        printf("Song Title: %s\n", mod.song_title);

        for (i = 0; i < 31; i++) {
            read_sample_record(&mod.samples[i], fp);
            printf("SAMPLE #%d\n", i + 1);
            print_sample(&mod.samples[i]);
        }

        printf("POSITION: %d\n", (int)ftell(fp));

        fread(&mod.song_length, sizeof(char), 1, fp);
        fseek(fp, 1, SEEK_CUR); /* Skip a (typically) unused byte */

        fread(mod.pattern_table, sizeof(char), 128, fp);

        printf("Song Length: %d\n", mod.song_length);
        printf("Pattern Table:\n");
        for (i = 0, mod.pattern_count = 0; i  < mod.song_length; i++) {
            if (mod.pattern_table[i] > mod.pattern_count)
                mod.pattern_count = mod.pattern_table[i];
            printf("\t%d\n", mod.pattern_table[i]);
        }

        printf("Pattern Count: %d\n", mod.pattern_count);

        fclose(fp);
    }
}
