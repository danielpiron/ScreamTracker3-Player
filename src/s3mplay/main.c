#include "portaudio.h"
#include "s3m.h"
#include <stdio.h>

#define SAMPLE_RATE 48000

int player_callback(
    const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{

    /* Prevent unused variable warnings. */
    (void)timeInfo;
    (void)statusFlags;
    (void)inputBuffer;
    (void)userData;

    /* Call scream tracker 3 audio stream generator */
    s3m_render_audio(outputBuffer, framesPerBuffer, (struct S3MPlayerContext*)userData);

    return paContinue;
}

int main(int argc, char* argv[])
{
    PaStreamParameters output_params;
    PaStream* stream;
    PaError err;

    struct S3MPlayerContext player;
    struct S3MFile s3m;

    char* filename;

    if (argc < 2) {
        fprintf(stderr, "Please enter a filename\n");
        return 1;
    }

    filename = argv[1];
    if (!s3m_load(&s3m, filename)) {
        fprintf(stderr, "Errors loading S3M File\n");
        return 1;
    }

    s3m_player_init(&player, &s3m, SAMPLE_RATE);

    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Error: Initializing PortAudio.\n");
        goto error;
    }

    output_params.device = Pa_GetDefaultOutputDevice();

    if (output_params.device == paNoDevice) {
        fprintf(stderr, "Error: No default output device.\n");
        goto error;
    }

    output_params.channelCount = 1; /* Mono */
    output_params.sampleFormat = paFloat32;
    output_params.suggestedLatency = Pa_GetDeviceInfo(output_params.device)->defaultLowInputLatency;
    output_params.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        NULL,
        &output_params,
        SAMPLE_RATE,
        1024,
        paClipOff,
        player_callback,
        &player);

    if (err != paNoError)
        goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError)
        goto error;

    printf("Press enter to exit...\n");
    getchar();

    err = Pa_StopStream(stream);
    if (err != paNoError)
        goto error;

    err = Pa_CloseStream(stream);
    if (err != paNoError)
        goto error;

    Pa_Terminate();

    return err;

error:
    Pa_Terminate();
    fprintf(stderr, "An error occured while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    return err;
}