#include "s3m.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATTERN_ROWS 64
#define PATTERN_CHANNELS 32
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

static struct S3MPatternEntry empty_note = { 0xFF, 0x00, 0xFF, 0xFF, 0x00 };

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

static char command_names[] = ".ABCDEFGHIJKLMNOPQRSTUVWXYZ";
/*
 * C-4 01 32 .00
 * 0123456789ABC
 * 14 characters including null terminator
 */
void render_pattern_entry(char *buf, const struct S3MPatternEntry* entry) {
    char empty_entry[] = "... .. .. .00";
    char digit_buf[16];
    strncpy(buf, empty_entry, 16);
    (void)entry;
    (void)digit_buf;
    (void)note_names;
    (void)command_names;

    if (entry->note != 255) {
        if (entry->note == 254) {
            memcpy(&buf[0], "---", 3);
        }
        else {
            int note = entry->note & 0x0F;
            int octave = entry->note >> 4;
            memcpy(&buf[0], note_names[note], 2);
            buf[2] = '0' + octave;
        }
    }
    if (entry->inst) {
      snprintf(digit_buf, sizeof(digit_buf), "%02d", entry->inst);
      memcpy(&buf[4], digit_buf, 2);
    }

    if (entry->vol != 255) {
      snprintf(digit_buf, sizeof(digit_buf), "%02d", entry->vol);
      memcpy(&buf[7], digit_buf, 2);
    }

    if (entry->command != 255) {
        buf[10] = command_names[entry->command];
        snprintf(digit_buf, sizeof(digit_buf), "%02X", entry->cominfo);
        memcpy(&buf[11], digit_buf, 2);
    }
}

void print_row(struct S3MPattern* pattern, int row) {
    int i;
    char buffer[16];
    char *prefix = "";
    for (i = 0; i < 8; i++) {
        render_pattern_entry(buffer, &pattern->row[row][i]);
        printf("%s%s", prefix, buffer);
        prefix = " | ";
    }
    printf("\n");
}

enum S3MEffect {
    ST3_EFFECT_UNUSED0 = 0, /* . */
    ST3_EFFECT_SET_SPEED, /* A */
    ST3_EFFECT_JUMP_TO_ORDER, /* B */
    ST3_EFFECT_BREAK_PATTERN, /* C */
    ST3_EFFECT_VOLUME_SLIDE, /* D */
    ST3_EFFECT_SLIDE_DOWN, /* E */
    ST3_EFFECT_SLIDE_UP, /* F */
    ST3_EFFECT_TONE_PORTAMENTO, /* G */
    ST3_EFFECT_VIBRATO, /* H */
    ST3_EFFECT_TREMOR, /* I */
    ST3_EFFECT_ARPEGGIO, /* J */
    ST3_EFFECT_VIBRATO_AND_VOLUME_SLIDE, /* K */
    ST3_EFFECT_PORTAMENTO_AND_VOLUME_SLIDE, /* L */
    ST3_EFFECT_UNUSED1,
    ST3_EFFECT_UNUSED2,
    ST3_EFFECT_SET_SAMPLE_OFFSET, /* O */
    ST3_EFFECT_UNUSED3,
    ST3_EFFECT_RETRIG, /* Q */
    ST3_EFFECT_TREMOLO, /* R */
    ST3_EFFECT_SPECIAL, /* S */
    ST3_EFFECT_TEMPO, /* T */
    ST3_EFFECT_FINE_VIBRATO, /* U */
    ST3_EFFECT_GLOBAL_VOLUME,/* V */
    ST3_EFFECT_EMPTY = 255
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
    return 14317456.0 / note_st3period;
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
    memset(ctx->channel, 0, sizeof(ctx->channel));

    /* TODO: Channels need to be distributed according to channel settings */
    for (i = 0; i < 16; i++)
        ctx->sample_stream[i].channel = &ctx->channel[i];

    /* Hardcode some default panning values */
    for (i = 0; i < 8; i++) {
        ctx->channel[i * 2].panning = 0x03; /* Even channels are left dominant */
        ctx->channel[i * 2 + 1].panning = 0x0C; /* Odd channels are right dominant */
    }

    printf("Current Pattern: %d\n", ctx->current_pattern);
}

void s3m_accumulate_sample_stream(float* buffer, int length, struct S3MSampleStream* ss, int sample_rate)
{
    struct S3MChannel* chan = ss->channel;
    float volume = chan->volume / 64.0;
    float panning = chan->panning / 15.0;

    if (volume == 0)
        return;

    ss->sample = chan->instrument;
    ss->sample_step = get_note_herz(chan->period) / sample_rate;
    if (chan->note_on) {
        ss->sample_index = 0;
        chan->note_on = 0;
    }

    while (length--) {

        ss->sample_index += ss->sample_step;

        /* If looping is enabled and we've reached the loop end, loop back. */
        if (ss->sample->header->flags & 1 && ss->sample_index >= ss->sample->header->loop_end)
            ss->sample_index -= (ss->sample->header->loop_end - ss->sample->header->loop_begin);

        if ((int)ss->sample_index < ss->sample->header->length) {
            float sample = (2.0 * ss->sample->sampledata[(int)ss->sample_index] / 255.0 - 1.0);
            /* Put the same sample into left and right channels to mimic mono */
            *buffer++ += (1.0 - panning) * volume * sample;
            *buffer++ += panning * volume * sample;
        }
    }
}

void s3m_process_tick(struct S3MPlayerContext* ctx)
{
    int c, x, y, last_row = 64;

    if (ctx->tick_counter == 0) {
        print_row(&ctx->patterns[ctx->current_pattern], ctx->current_row);
        for (c = 0; c < 16; c++) {

            struct S3MPatternEntry* entry = &ctx->patterns[ctx->current_pattern].row[ctx->current_row][c];

            if (entry->note != 0xFF && entry->note != 0xFE) {
                if (entry->inst) {
                    ctx->channel[c].instrument = &ctx->file->instruments[entry->inst - 1];
                    ctx->channel[c].volume = (entry->vol == 0xFF)
                        ? ctx->channel[c].instrument->header->default_volume
                        : entry->vol;
                }
                if (ctx->channel[c].instrument != NULL) {
                    if (entry->command == ST3_EFFECT_TONE_PORTAMENTO) {
                        ctx->channel[c].effects.portamento_target = get_note_st3period(entry->note, ctx->channel[c].instrument->header->c2_speed);
                    } else {
                        ctx->channel[c].effects.vibrato.position = 0;
                        ctx->channel[c].period = get_note_st3period(entry->note, ctx->channel[c].instrument->header->c2_speed);
                        ctx->channel[c].effects.vibrato.old_period = ctx->channel[c].period;
                        ctx->channel[c].note_on = 1;
                    }
                }
            } else {
                if (entry->vol != 0xFF)
                    ctx->channel[c].volume = entry->vol;

                if (entry->note == 0xFE)
                    /* Cheap note cut by setting volume to 0. */
                    ctx->channel[c].volume = 0;
            }


            if (ctx->channel[c].current_effect != ST3_EFFECT_VIBRATO
                && entry->command == ST3_EFFECT_VIBRATO) {
                ctx->channel[c].effects.vibrato.old_period = ctx->channel[c].period;
            }

            if (ctx->channel[c].current_effect == ST3_EFFECT_VIBRATO
                && entry->command != ST3_EFFECT_VIBRATO) {
                ctx->channel[c].period = ctx->channel[c].effects.vibrato.old_period;
            }

            x = entry->cominfo >> 4;
            y = entry->cominfo & 15;
            switch (entry->command) {
            case ST3_EFFECT_SET_SPEED:
                ctx->song_speed = entry->cominfo;
                break;
            case ST3_EFFECT_BREAK_PATTERN:
                last_row = ctx->current_row + 1;
                break;
            case ST3_EFFECT_PORTAMENTO_AND_VOLUME_SLIDE:
            case ST3_EFFECT_VIBRATO_AND_VOLUME_SLIDE:
            case ST3_EFFECT_VOLUME_SLIDE:
                if (entry->cominfo) {
                    if (y && (x == 0 || x == 15)) {
                        ctx->channel[c].effects.volume_slide_speed = -y;
                        ctx->channel[c].effects.is_fine_slide = (x == 15);
                    }
                    else if (x && (y == 0 || y == 15)) {
                        ctx->channel[c].effects.volume_slide_speed = x;
                        ctx->channel[c].effects.is_fine_slide = (y == 15);
                    }
                }
                ctx->channel[c].current_effect = entry->command;
                break;
            case ST3_EFFECT_SLIDE_DOWN: /* E */
            case ST3_EFFECT_SLIDE_UP: /* F */
                if (entry->cominfo) {
                    if (x == 15) {
                        ctx->channel[c].effects.pitch_slide_type = 1;
                        ctx->channel[c].effects.pitch_slide_speed = y;
                    }
                    else if (x == 14) {
                        ctx->channel[c].effects.pitch_slide_type = 2;
                        ctx->channel[c].effects.pitch_slide_speed = y;
                    }
                    else {
                        ctx->channel[c].effects.pitch_slide_type = 0;
                        ctx->channel[c].effects.pitch_slide_speed = entry->cominfo;
                    }
                }
                ctx->channel[c].current_effect = entry->command;
                break;
            case ST3_EFFECT_TONE_PORTAMENTO:
                if (entry->cominfo)
                    ctx->channel[c].effects.portamento_speed = entry->cominfo;

                ctx->channel[c].current_effect = ST3_EFFECT_TONE_PORTAMENTO;
                break;
            case ST3_EFFECT_VIBRATO:
                if (entry->cominfo) {
                    ctx->channel[c].effects.vibrato.speed = x;
                    ctx->channel[c].effects.vibrato.depth = y;
                }
                ctx->channel[c].current_effect = ST3_EFFECT_VIBRATO;
                break;
            case ST3_EFFECT_RETRIG:
                if (entry->cominfo) {
                    ctx->channel[c].effects.retrig_frequency = y;
                }
                ctx->channel[c].effects.retrig_counter = 0;
                ctx->channel[c].current_effect = ST3_EFFECT_RETRIG;
                break;
            default:
                ctx->channel[c].current_effect = 0;
            }

        }
        ctx->current_row++;
        if (ctx->current_row == last_row) {
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

    for (c = 0; c < 16; c++) {
        if (ctx->channel[c].current_effect == ST3_EFFECT_VOLUME_SLIDE
            || ctx->channel[c].current_effect == ST3_EFFECT_PORTAMENTO_AND_VOLUME_SLIDE
            || ctx->channel[c].current_effect == ST3_EFFECT_VIBRATO_AND_VOLUME_SLIDE) {
            int perform_slide = 0;

            if (ctx->tick_counter == ctx->song_speed)
                perform_slide = ctx->channel[c].effects.is_fine_slide;
            else
                perform_slide = !ctx->channel[c].effects.is_fine_slide;

            if (perform_slide) {
                ctx->channel[c].volume += ctx->channel[c].effects.volume_slide_speed;
                if (ctx->channel[c].volume > 64) ctx->channel[c].volume = 64;
                if (ctx->channel[c].volume < 0) ctx->channel[c].volume = 0;
            }
        }
        if (ctx->channel[c].current_effect == ST3_EFFECT_VIBRATO
            || ctx->channel[c].current_effect == ST3_EFFECT_VIBRATO_AND_VOLUME_SLIDE) {
            if (ctx->tick_counter != ctx->song_speed) {
                int s = 64 * sin(2 * M_PI * ((ctx->channel[c].effects.vibrato.position & 0xFF) / 255.0));
                int delta = (4 * ctx->channel[c].effects.vibrato.depth * s) >> 6;
                ctx->channel[c].period +=  delta;
                ctx->channel[c].effects.vibrato.position += ctx->channel[c].effects.vibrato.speed * 4;
            }
        }

        if (ctx->channel[c].current_effect == ST3_EFFECT_SLIDE_UP) {
            if (ctx->channel[c].effects.pitch_slide_type) {
                if (ctx->tick_counter == ctx->song_speed) {
                    int factor = (ctx->channel[c].effects.pitch_slide_type == 2) ? 1 : 4;
                    ctx->channel[c].period -= ctx->channel[c].effects.pitch_slide_speed * factor;
                }
            }
            else if (ctx->tick_counter != ctx->song_speed)
                ctx->channel[c].period -= ctx->channel[c].effects.pitch_slide_speed * 4;
        }
        if (ctx->channel[c].current_effect == ST3_EFFECT_SLIDE_DOWN) {
            if (ctx->channel[c].effects.pitch_slide_type) {
                if (ctx->tick_counter == ctx->song_speed) {
                    int factor = (ctx->channel[c].effects.pitch_slide_type == 2) ? 1 : 4;
                    ctx->channel[c].period += ctx->channel[c].effects.pitch_slide_speed * factor;
                }
            }
            else if (ctx->tick_counter != ctx->song_speed)
                ctx->channel[c].period += ctx->channel[c].effects.pitch_slide_speed * 4;
        }
        if (ctx->channel[c].current_effect == ST3_EFFECT_TONE_PORTAMENTO
            || ctx->channel[c].current_effect == ST3_EFFECT_PORTAMENTO_AND_VOLUME_SLIDE) {
           if (ctx->channel[c].period < ctx->channel[c].effects.portamento_target) {
               ctx->channel[c].period += ctx->channel[c].effects.portamento_speed * 4;
               if (ctx->channel[c].period > ctx->channel[c].effects.portamento_target)
                   ctx->channel[c].period = ctx->channel[c].effects.portamento_target;
           }
           else if (ctx->channel[c].period > ctx->channel[c].effects.portamento_target) {
               ctx->channel[c].period -= ctx->channel[c].effects.portamento_speed * 4;
               if (ctx->channel[c].period < ctx->channel[c].effects.portamento_target)
                   ctx->channel[c].period = ctx->channel[c].effects.portamento_target;
           }
        }
        if (ctx->channel[c].current_effect == ST3_EFFECT_RETRIG) {
            if (ctx->channel[c].effects.retrig_counter++ == ctx->channel[c].effects.retrig_frequency) {
                ctx->channel[c].note_on = 1;
                ctx->channel[c].effects.retrig_counter = 0;
            }
        }
    }
    ctx->tick_counter--;
}

void s3m_render_audio(float* buffer, int samples_remaining, struct S3MPlayerContext* ctx)
{
    while (samples_remaining) {
        int samples_to_render;
        int i;
        if (ctx->samples_until_next_tick == 0) {
            s3m_process_tick(ctx);
            ctx->samples_until_next_tick = ctx->samples_per_tick;
        }

        samples_to_render = (samples_remaining > ctx->samples_until_next_tick)
            ? ctx->samples_until_next_tick
            : samples_remaining;

        samples_remaining -= samples_to_render;
        ctx->samples_until_next_tick -= samples_to_render;

        memset(buffer, 0, sizeof(float) * samples_to_render * 2);

        for (i = 0; i < 16; i++)
            s3m_accumulate_sample_stream(buffer, samples_to_render, &ctx->sample_stream[i], ctx->sample_rate);

        for (i = 0; i < samples_to_render * 2; i++)
            buffer[i] /= 8.0;

        buffer += samples_to_render * 2;
    }
}
