#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <rtl-sdr.h>

rtlsdr_dev_t* rtlsdr;
FILE* outf;
int async_started;
int quit_please;
int buffers_received;

void sighandler(int signal)
{
    printf("Caught ctrl-c, cleaning up.\n");
    quit_please = 1;
    if(!async_started) {
        if(outf != NULL) {
            fclose(outf);
        }
        exit(0);
    }
}

void read_callback(unsigned char* buf, uint32_t len, void* ctx)
{
    printf(".");
    fflush(stdout);
    fwrite(buf, 1, len, outf);
    buffers_received++;

    if(quit_please || buffers_received == 20) {
        printf("Final callback, shutting down.\n");

        printf("Closing output file...\n");
        fclose(outf);

        printf("Stopping RTL-SDR streaming...\n");
        rtlsdr_cancel_async(rtlsdr);

        exit(0);

        printf("Closing RTL-SDR...\n");
        rtlsdr_close(rtlsdr);

        exit(0);
    }
}

int main(int argc, char* argv[])
{
    int rv;
    async_started = 0;
    quit_please = 0;
    buffers_received = 0;

    signal(SIGINT, sighandler);

    printf("Opening output file...\n");
    outf = fopen("rtlsdr_out.bin", "w");
    if(outf == NULL) {
        printf("Error opening output file: %d.\nExiting.\n", errno);
        return 1;
    }

    rv = rtlsdr_get_device_count();

    if(rv == 0) {
        printf("No RTL-SDR devices found, exiting.\n");
        return 2;
    }

    printf("Found %d device(s).\n", rv);

    printf("Opening the first, '%s'...\n", rtlsdr_get_device_name(0));
    rv = rtlsdr_open(&rtlsdr, 0);
    if(rv != 0) {
        printf("Error opening device: %d\nExiting.\n", rv);
        return 3;
    }

    printf("Setting frequency to 315MHz...\n");
    rv = rtlsdr_set_center_freq(rtlsdr, 315000000);
    if(rv != 0) {
        printf("Error setting frequency: %d\nExiting.\n", rv);
        return 4;
    }
    printf("Frequency set to %uHz.\n", rtlsdr_get_center_freq(rtlsdr));

    printf("Setting gain mode to automatic.\n");
    rv = rtlsdr_set_tuner_gain_mode(rtlsdr, 0);
    if(rv != 0) {
        printf("Error setting gain mode: %d\nExiting.\n", rv);
        return 5;
    }
    printf("Gain currently set to %d.\n", rtlsdr_get_tuner_gain(rtlsdr));

    printf("Setting sample rate to 240kHz...\n");
    rv = rtlsdr_set_sample_rate(rtlsdr, 240000);
    if(rv != 0) {
        printf("Error setting sample rate: %d\nExiting.\n", rv);
        return 6;
    }
    printf("Sample rate set to %u.\n", rtlsdr_get_sample_rate(rtlsdr));

    printf("Setting AGC on...\n");
    rv = rtlsdr_set_agc_mode(rtlsdr, 1);
    if(rv != 0) {
        printf("Error setting AGC: %d\nExiting.\n", rv);
        return 7;
    }
    
    
    printf("Clearing buffer and streaming data...\n");
    rv = rtlsdr_reset_buffer(rtlsdr);
    if(rv != 0) {
        printf("Error clearing buffer: %d\nExiting.\n", rv);
        return 8;
    }

    async_started = 1;

    rv = rtlsdr_read_async(rtlsdr, read_callback, NULL, 0, 0);
    if(rv != 0) {
        printf("Error setting up async streaming: %d\nExiting.\n", rv);
        return 9;
    }

    rtlsdr_cancel_async(rtlsdr);
    rtlsdr_close(rtlsdr);
    return 0;

}
