#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#define BUFSIZE 262144
#define FS 240000
#define BD 300

FILE* inf;
FILE* outf;
int start_position;
int state;
int bitbuffer[8];

float filter[16] = {
    0.00977160048227, 0.0146329412836, 0.0283755459692, 0.048627651374,
    0.071889216771,   0.0941376629054, 0.111524339992,  0.121041041222,
    0.121041041222,   0.111524339992,  0.0941376629054, 0.071889216771,
    0.048627651374,   0.0283755459692, 0.0146329412836, 0.00977160048227};

void rs232(float* data, float threshold)
{
    int i, j;
    uint8_t out_byte;

    if(state == -1) {
        // Find first start bit
        start_position = 0;
        for(i=20; i<BUFSIZE/2; i++) {
            if(data[i] < threshold) {
                start_position = i;
                break;
            }
        }

        if(start_position <= 20) {
            printf("Start bit not found, skipping buffer.\n");
            return;
        }

        printf("Start bit: %d\n", start_position);

        for(i=0; i<8; i++)
            bitbuffer[i] = 0;

    }

    // Go through each sample point, decoding bytes as we progress.
    // State -1 is "waiting for a start bit",
    // State 0 through 7 is "about to record bit 7..0",
    // State 8 is "waiting for a stop bit"
    for(i=start_position + ((FS/BD)/2); i<BUFSIZE/2; i+=FS/BD) {
        if(state == -1) {
            if(data[i] < threshold) {
                state = 0;
                printf("START\n");
            }
        } else if(state >= 0 && state < 8) {
            bitbuffer[state] = data[i] > threshold ? 1 : 0;
            printf("%d ", bitbuffer[state]);
            state++;
        } else if(state == 8) {
            if(data[i] > threshold) {
                state = -1;
                printf("\nEND\n");
                out_byte = 0;
                for(j=0; j<8; j++) {
                    out_byte |= bitbuffer[j] << j;
                    bitbuffer[j] = 0;
                }
                printf("Writing out byte: %c\n", out_byte);
                fputc(out_byte, outf);
            } else {
                printf("Bad state. Ugh.\n");
                state = -1;
            }
        }
    }

    if(state >= 0 && state < 8)
        printf("\n");

    start_position = (BUFSIZE/2 - start_position)
                     - (((BUFSIZE/2 - start_position) / (FS/BD))
                        * (FS/BD))
                     - (FS/BD)/2;
    printf("New start position: %d\n", start_position);
}

void decode(uint8_t* buf)
{
    int i, j;
    float data[BUFSIZE/2];
    float x[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float threshold, real, imag;
    double acc, acc_real, acc_imag;

    // Find the mean of the I and Q components
    acc_real = acc_imag = 0;
    for(i=0; i<BUFSIZE; i+=2) {
        acc_real += (double)buf[i];
        acc_imag += (double)buf[i+1];
    }
    acc_real /= BUFSIZE/2;
    acc_imag /= BUFSIZE/2;

    // Remove the mean then take the magnitude
    for(i=0; i<BUFSIZE; i+=2) {
        real = (float)buf[i] - acc_real;
        imag = (float)buf[i+1] - acc_imag;
        data[i/2] = sqrt(pow(real, 2) + pow(imag, 2));
    }

    // Perform low pass filter
    for(i=0; i<BUFSIZE/2; i++) {
        for(j=1; j<16; j++)
            x[j] = x[j-1];
        x[0] = data[i];

        acc = 0.0;
        for(j=0; j<16; j++)
            acc += x[j] * filter[j];
        data[i] = acc;
    }

    // Calculate threshold (mean/2)
    acc = 0.0;
    for(i=0; i<BUFSIZE/2; i++) {
        acc += data[i];
    }
    threshold = (acc / (BUFSIZE/2)) / 2;

    printf("Threshold: %f\n", threshold);

    rs232(data, threshold);
}

int main(int argc, char* argv[])
{
    int i;
    uint8_t fbuf[1310720];
    uint8_t *buf;
    inf = fopen("rtlsdr_out.bin", "r");
    outf = fopen("rtlsdr_offline.txt", "w");
    fread(fbuf, 1, 1310720, inf);
    fclose(inf);

    state = -1;
    for(i=0; i<5; i++) {
        buf = fbuf + (BUFSIZE * i);
        decode(buf);
    }

    fclose(outf);

    return 0;
}
