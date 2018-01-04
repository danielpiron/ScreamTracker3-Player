#include "s3m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATTERN_ROWS 64
#define PATTERN_CHANNELS 32

static struct S3MPatternEntry empty_note = { 0xFF, 0x00, 0xFF, 0xFF, 0x00 };

enum S3MEffect {
    ST3_EFFECT_EMPTY = 0,
    ST3_EFFECT_SET_SPEED,
    ST3_EFFECT_JUMP_TO_ORDER,
    ST3_EFFECT_BREAK_PATTERN,
    ST3_EFFECT_VOLUME_SLIDE,
    ST3_EFFECT_SLIDE_DOWN,
    ST3_EFFECT_SLIDE_UP,
    ST3_EFFECT_TONE_PORTAMENTO,
    ST3_EFFECT_VIBRATO,
    ST3_EFFECT_TREMOR,
    ST3_EFFECT_ARPEGGIO,
    ST3_EFFECT_VIBRATO_AND_VOLUME_SLIDE,
    ST3_EFFECT_PORTAMENTO_AND_VOLUME_SLIDE,
    ST3_EFFECT_SET_SAMPLE_OFFSET,
    ST3_EFFECT_UNUSED1,
    ST3_EFFECT_RETRIG,
    ST3_EFFECT_TREMOLO,
    ST3_EFFECT_SPECIAL,
    ST3_EFFECT_TEMPO,
    ST3_EFFECT_FINE_VIBRATO,
    ST3_EFFECT_GLOBAL_VOLUME
};

static const int st3period_table[] = {
    1712, /* C  */
    1616, /* C# */
    1524, /* D  */
    1440, /* D# */
    1356, /* E  */
    1280, /* F  */
    1208, /* F# */
    1140, /* G  */
    1076, /* G# */
    1016, /* A  */
    960, /* A# */
    907 /* B  */
};

static int get_note_st3period(int raw_note, int c2speed)
{
    int octave = raw_note >> 4;
    int note = raw_note & 0x0F;
    int base_period = st3period_table[note < 12 ? note : 11];
    return 8363 * (16 * base_period >> octave) / c2speed;
}

static float get_note_herz(int note_st3period)
{
    return 14317056.0 / note_st3period;
}

static void s3m_player_set_sample_period(struct S3MPlayerContext* ctx, struct S3MSampleStream* ss, int period)
{
    /* TODO: Assert period > 0 */
    ss->sample_period = period;
    ss->sample_step = get_note_herz(ss->sample_period) / ctx->sample_rate;
    ss->sample_index = 0;
}

static void s3m_player_set_tempo(struct S3MPlayerContext* ctx, int tempo)
{
    ctx->song_tempo = tempo;
    ctx->samples_per_tick = 2.5 / tempo * ctx->sample_rate;
}

void s3m_pattern_unpack(struct S3MPattern* pattern, struct S3MPackedPattern* packed)
{
    unsigned char* data = packed->data;
    unsigned char* data_end = &data[packed->length];
    int channel = 0;
    int row = 0;
    while (data < data_end && row < 64) {
        struct S3MPatternEntry entry = empty_note;
        unsigned char flags;
        if (*data == 0) {
            data++;
            row++;
            continue;
        }
        channel = *data & 0x1F;
        flags = *data & 0xE0;
        data++;
        if (flags & 0x20) {
            entry.note = *data++;
            entry.inst = *data++;
        }
        if (flags & 0x40) {
            entry.vol = *data++;
        }
        if (flags & 0x80) {
            entry.command = *data++;
            entry.cominfo = *data++;
        }
        pattern->row[row][channel] = entry;
    }
}

void s3m_pattern_init(struct S3MPattern* pattern)
{
    int i, j;
    for (i = 0; i < PATTERN_ROWS; i++) {
        for (j = 0; j < PATTERN_CHANNELS; j++) {
            pattern->row[i][j] = empty_note;
        }
    }
}

void s3m_player_init(struct S3MPlayerContext* ctx, struct S3MFile* file, int sample_rate)
{
    int i;

    memset(ctx, 0, sizeof(struct S3MPlayerContext));

    ctx->file = file;
    ctx->sample_rate = sample_rate;

    /* Default settings */
    ctx->current_row = 0;
    ctx->song_speed = 6;
    ctx->tick_counter = ctx->song_speed;
    ctx->samples_until_next_tick = 0;
    ctx->sample_rate = 48000;
    s3m_player_set_tempo(ctx, 125);

    /* Allocate space for pattern data */
    ctx->patterns = malloc(sizeof(struct S3MPattern) * file->header->pattern_count);
    for (i = 0; i < file->header->pattern_count; i++) {
        s3m_pattern_init(&ctx->patterns[i]);
        s3m_pattern_unpack(&ctx->patterns[i], &file->packed_patterns[i]);
    }

    ctx->current_order = 0;
    ctx->current_pattern = file->orders[ctx->current_order];

    memset(ctx->sample_stream, 0, sizeof(ctx->sample_stream));

    printf("Current Pattern: %d\n", ctx->current_pattern);
}

void s3m_accumulate_sample_stream(float* buffer, int length, struct S3MSampleStream* ss)
{
    if (ss->sample == NULL)
        return;

    while (length--) {

        ss->sample_index += ss->sample_step;

        /* If looping is enabled and we've reached the loop end, loop back. */
        if (ss->sample->header->flags & 1 && ss->sample_index >= ss->sample->header->loop_end)
            ss->sample_index -= (ss->sample->header->loop_end - ss->sample->header->loop_begin);

        if (ss->volume > 0 && (int)ss->sample_index < ss->sample->header->length)
            *buffer++ += ss->volume * (2.0 * ss->sample->sampledata[(int)ss->sample_index] / 255.0 - 1.0);
    }
}

void handle_effects(struct S3MPlayerContext* ctx, int channel, enum S3MEffect effect, int data)
{
    (void)channel;
    switch (effect) {
    case ST3_EFFECT_SET_SPEED:
        ctx->song_speed = data;
        break;
    default:
        return;
    }
}

void s3m_render_audio(float* buffer, int samples_remaining, struct S3MPlayerContext* ctx)
{
    while (samples_remaining) {

        int samples_to_render;
        int c;

        if (ctx->tick_counter == 0) {

            for (c = 0; c < 8; c++) {

                struct S3MPatternEntry* entry = &ctx->patterns[ctx->current_pattern].row[ctx->current_row][c];

                if (entry->note != 0xFF && entry->note != 0xFE) {
                    if (entry->inst) {
                        ctx->sample_stream[c].current_inst = entry->inst - 1;
                        ctx->sample_stream[c].sample = &ctx->file->instruments[ctx->sample_stream[c].current_inst];
                        ctx->sample_stream[c].volume = ((entry->vol == 0xFF)
                                                               ? ctx->sample_stream[c].sample->header->default_volume
                                                               : entry->vol)
                            / 64.0;
                    }
                    if (ctx->sample_stream[c].sample)
                        s3m_player_set_sample_period(ctx, &ctx->sample_stream[c],
                            get_note_st3period(entry->note,
                                                         ctx->sample_stream[c].sample->header->c2_speed));
                } else {
                    if (entry->vol != 0xFF)
                        ctx->sample_stream[c].volume = entry->vol / 64.0;

                    if (entry->note == 0xFE)
                        /* Cheap note cut by setting volume to 0. */
                        ctx->sample_stream[c].volume = 0;
                }

                handle_effects(ctx, c, entry->command, entry->cominfo);
            }
            ctx->current_row++;
            if (ctx->current_row == PATTERN_ROWS) {
                ctx->current_order++;
                /* If we've reached the last order repeat song */
                if (ctx->file->orders[ctx->current_order] == 0xFF)
                    ctx->current_order = 0;

                ctx->current_pattern = ctx->file->orders[ctx->current_order];
                printf("Current Pattern: %d\n", ctx->current_pattern);
                ctx->current_row = 0;
            }
            ctx->tick_counter = ctx->song_speed;
        }

        if (ctx->samples_until_next_tick == 0) {
            ctx->tick_counter--;
            ctx->samples_until_next_tick = ctx->samples_per_tick;
        }

        samples_to_render = (samples_remaining > ctx->samples_until_next_tick)
            ? ctx->samples_until_next_tick
            : samples_remaining;

        samples_remaining -= samples_to_render;
        ctx->samples_until_next_tick -= samples_to_render;

        memset(buffer, 0, sizeof(float) * samples_to_render);

        for (c = 0; c < 8; c++)
            s3m_accumulate_sample_stream(buffer, samples_to_render, &ctx->sample_stream[c]);

        for (c = 0; c < samples_to_render; c++)
            buffer[c] /= 8.0;

        buffer += samples_to_render;
    }
}

static int _s3m_file_is_valid(struct S3MFile* s3m)
{

    if (s3m->header->magic_num == 0x1a
        && s3m->header->type == 16
        && memcmp(s3m->header->SCRM, "SCRM", 4) == 0) {
        return 1;
    }

    fprintf(stderr, "S3M File invalid\n");
    return 0;
}

static int _s3m_load(struct S3MFile* s3m, FILE* fp)
{
    long filesize;

    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);

    /* Dump entire file's contents into memory,
     * because it's the 21st century. */
    s3m->file_data = malloc(filesize);
    if (fread(s3m->file_data, 1, filesize, fp)) {
        unsigned short* parapointers;
        int i;

        s3m->header = (struct S3MModuleHeader*)s3m->file_data;
        s3m->orders = &s3m->file_data[0x60];

        if (!_s3m_file_is_valid(s3m)) {
            fprintf(stderr, "S3M File is invalid\n");
            free(s3m->file_data);
            return 0;
        }

        /* Setup instrument pointers */
        parapointers = (unsigned short*)&s3m->file_data[0x60 + s3m->header->order_count];
        for (i = 0; i < s3m->header->instrument_count; i++) {
            struct S3MSampleInstrument* inst = &s3m->instruments[i];
            inst->header = (struct S3MSampleHeader*)&s3m->file_data[parapointers[i] * 16];
            inst->sampledata = (unsigned char*)&s3m->file_data[inst->header->sample_data_parapointer * 16];
        }

        parapointers = (unsigned short*)&s3m->file_data[0x60
            + s3m->header->order_count
            + s3m->header->instrument_count * 2];
        for (i = 0; i < s3m->header->pattern_count; i++) {
            unsigned short* packed_length = (unsigned short*)&s3m->file_data[parapointers[i] * 16];
            s3m->packed_patterns[i].length = *packed_length;
            /* Packed data begins 2 bytes after length (WORD) */
            s3m->packed_patterns[i].data = (unsigned char*)&packed_length[1];
        }

        printf("S3M Load Complete\n");
        return 1;
    }
    fprintf(stderr, "Error loading file\n");
    free(s3m->file_data);
    return 0;
}

int s3m_load(struct S3MFile* s3m, const char* filename)
{
    FILE* fp;
    int status;
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Can't open file: %s\n", filename);
        return 0;
    }
    status = _s3m_load(s3m, fp);

    fclose(fp);
    return status;
}
