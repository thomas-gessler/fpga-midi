/* Output:
 *     28-bit delta
 *     1-bit enable
 *     7-bit note
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#define MAXLEN (65536)

int main()
{
    char header[5] = { 0 };
    unsigned char buf[MAXLEN];
    uint32_t len;
    uint32_t pos;

    uint32_t delta;

    ssize_t rbytes;

    unsigned meta_len;
    unsigned meta_pos;

    uint8_t status_or_running;
    uint8_t status;

    uint8_t note;
    uint8_t last_note;

    uint64_t total_time;
    uint64_t last_time;

    int off_valid;
    uint64_t off_time;

    for (;;) {
        rbytes = read(0, header, 4);
        if (rbytes == 0) {
            fprintf(stderr, "Done.\n");
            exit(EXIT_SUCCESS);
        } else if (rbytes != 4) {
            fprintf(stderr, "Header read error.\n");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "%s\n", header);

        rbytes = read(0, &len, 4);
        if (rbytes != 4) {
            fprintf(stderr, "Length read error.\n");
            exit(EXIT_FAILURE);
        }

        len = ntohl(len);

        if (len > MAXLEN) {
            fprintf(stderr, "Section too long.\n");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "%d\n", len);

        rbytes = read(0, buf, len);
        if (rbytes != len) {
            fprintf(stderr, "Section read error.\n");
            exit(EXIT_FAILURE);
        }

        if (!strncmp(header, "MThd", 4))
            continue;

        pos = 0;
        total_time = 0;
        last_time = 0;
        off_valid = 0;
        while (pos < len) {
            delta = 0;
            do {
                delta = (delta << 7) + (buf[pos++] & 0x7F);
            } while (buf[pos - 1] & 0x80);

            fprintf(stderr, "+%4d ", delta);
            if (delta > 0 && off_valid) {
                printf("%05X00\n", (uint32_t)(off_time));
                last_time = off_time;
                off_valid = 0;
            }
            total_time += delta;

            status_or_running = buf[pos++];

            if (status_or_running & 0x80) {
                status = status_or_running;
            } else {
                fprintf(stderr, "Running\n");
                --pos;
            }

            if (status == 0xFF) {
                fprintf(stderr, "Meta %02X ", buf[pos++]);

                meta_len = 0;
                do {
                    meta_len = (meta_len << 7) + (buf[pos++] & 0x7F);
                } while (buf[pos - 1] & 0x80);

                fprintf(stderr, "len %d: ", meta_len);

                for (meta_pos = 0; meta_pos < meta_len; ++meta_pos)
                    fprintf(stderr, "%02x ", buf[pos++]);

                fprintf(stderr, "\n");
            } else if ((status & 0xF0) == 0x80) {
                fprintf(stderr, "Note OFF ch %d: ", status & 0x0F);
                note = buf[pos++];
                fprintf(stderr, "note %d ", note);
                fprintf(stderr, "vel %d\n", buf[pos++]);

                if (note == last_note) {
                    off_valid = 1;
                    off_time = total_time;
                }
            } else if ((status & 0xF0) == 0x90) {
                fprintf(stderr, "Note ON  ch %d: ", status & 0x0F);
                last_note = buf[pos++];
                fprintf(stderr, "note %d ", last_note);
                fprintf(stderr, "vel %d\n", buf[pos++]);

                off_valid = 0;

                printf("%05X%02X\n", (uint32_t)(total_time), 0x80 | last_note);
                last_time = total_time;
            } else if ((status & 0xF0) == 0xC0) {
                fprintf(stderr, "Program ch %d: %d\n", status & 0x0F, buf[pos++]);
            } else if ((status & 0xF0) == 0xB0) {
                fprintf(stderr, "Controller ch %d ", status & 0x0F);
                fprintf(stderr, "num %d: ", buf[pos++]);
                fprintf(stderr, "%d\n", buf[pos++]);
            } else {
                fprintf(stderr, "Status %02X\n", status);
            }
        }
        if (off_valid) {
            printf("%05X00", (uint32_t)(off_time));
            last_time = off_time;
            off_valid = 0;
        }
        fprintf(stderr, "\n");
        printf("\n");
    }

    return -1;
}
