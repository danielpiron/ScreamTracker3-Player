#include "s3m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
