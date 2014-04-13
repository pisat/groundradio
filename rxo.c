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

/*
 * a 16-tap FIR LPF filter at 400Hz
 * scipy.signal.firwin(numtaps=16, cutoff=400.0, nyq=240000/2.0)
 */
float filter[16] = {
    0.00977160048227, 0.0146329412836, 0.0283755459692, 0.048627651374,
    0.071889216771,   0.0941376629054, 0.111524339992,  0.121041041222,
    0.121041041222,   0.111524339992,  0.0941376629054, 0.071889216771,
    0.048627651374,   0.0283755459692, 0.0146329412836, 0.00977160048227};

/*
 * RS232 decoding state machine thing.
 * Uses global start_position to work out where in the dataset to start
 * looking, useful when we'd already got sync in a previous buffer and wish to
 * keep it between buffers.
 * Uses global state to determine if it's waiting for a start bit, reading a
 * specific data bit or waiting for a stop bit.
 * Uses global bitbuffer to hold the eight bits that'l make up the next output
 * byte.
 * Writes to outf.
 *
 * If state is -1, it will look for the first start bit it sees (transition
 * low->high), possibly involving skipping this buffer entirely if none are
 * found.
 */
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

        // Start bit not found so skip buffer
        if(start_position <= 20) {
            return;
        }
    }

    // Go through each sample point, decoding bytes as we progress.
    // State -1 is "waiting for a start bit",
    // State 0 through 7 is "about to record bit 7..0",
    // State 8 is "waiting for a stop bit"
    for(i=start_position + ((FS/BD)/2); i<BUFSIZE/2; i+=FS/BD) {
        if(state == -1) {
            if(data[i] < threshold) {
                state = 0;
            }
        } else if(state >= 0 && state < 8) {
            bitbuffer[state] = data[i] > threshold ? 1 : 0;
            state++;
        } else if(state == 8) {
            if(data[i] > threshold) {
                // Yay, byte done, save it and move on to the next one.
                state = -1;
                out_byte = 0;
                for(j=0; j<8; j++) {
                    out_byte |= bitbuffer[j] << j;
                    bitbuffer[j] = 0;
                }
                fputc(out_byte, outf);
            } else {
                // This shouldn't happen, so feel bad.
                state = -1;
            }
        }
    }

    // This looks dubiously complicated but...
    // we want to work out how many samples into the next buffer the next
    // symbol will start. We can take all the samples since our buffer started,
    // subtract all the samples for symbols we covered, subtract a bit more to
    // compensate for how we'll be covering any symbols that have their halfway
    // point inside our buffer, then add one to begin the next symbol.
    // It works, honest!
    start_position = (BUFSIZE/2 - start_position)
                     - (((BUFSIZE/2 - start_position) / (FS/BD))
                        * (FS/BD))
                     - (FS/BD)/2 + 1;
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

    // Decode data
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
