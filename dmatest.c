#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <ext.h>
#include <mint/osbind.h>

static long errcount = 0;

__attribute__((noreturn))
static void quit(char is_err, const char *fmt, ...)
{
	va_list ap;
	FILE *stream = stdout;
	int ret = EXIT_SUCCESS;

	if (is_err) {
		stream = stderr;
		ret = EXIT_FAILURE;
	}

	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);

	printf("\r\nPress any key to exit.");
	getchar();
	exit(ret);
}


/* format the string to eat up the whole line
 * hack: Atari low resolution is 40 characters per line */
static void print_full_line(char is_err, const char *fmt, ...) {
	static char line[42];
	va_list ap;

	line[0] = '\r';

	va_start(ap, fmt);
	char n = vsnprintf(line + 1, sizeof(line) - 1, fmt, ap);
	va_end(ap);

	if (n < (char)(sizeof(line) - 2)) {
		memset(line + 1 + n, ' ', sizeof(line) - n - 2);
		line[sizeof(line)-1] = 0;
	}

	fprintf(is_err ? stderr : stdout, line);
}

static void check_user_interruption(void)
{
	if (Cconis()) {
		/* user keyboard interrupt */
		getchar();
		print_full_line(1, "Test interrupted. %ld errors detected.", errcount);
		quit(1, "");
	}
}

int main(int argc, char **argv)
{
	char drive, dev;
	int opt;

	printf("DMATEST by benou - ben@benou.fr\r\n\r\n");

	printf("Floppy drive to test? [a|b]");
	drive = toupper(getchar());
	switch (drive) {
		case 'A':
			dev = 0;
			break;
		case 'B':
			dev = 1;
			break;
		default:
			quit(1, "\r\nDrive %c not supported.", drive);
	}

	print_full_line(1, "WARNING - DESTRUCTIVE TEST");
	fprintf(stderr, "\r\nAny data saved on floppy %c will be lost!\r\n", drive);
	fprintf(stderr, "Continue? [y/n] ");
	switch(getchar()) {
		case 'y':
		case 'Y':
			/* accept */
			printf("\bYES\r\n\n");
			break;
		default:
			/* user backed off, exit */
			quit(0, "\bNO");
	}

	const _BPB *bpb = Getbpb(dev);
	if (!bpb)
		quit(1, "Failed to read BPB for dev %d.", dev);

	if (bpb->recsiz * bpb->clsiz != bpb->clsizb)
		quit(1, "Inconsistent BPB detected for dev %d.", dev);

	short totsec = bpb->numcl * bpb->clsiz;

	printf("Floppy: %c\r\n", drive);
	printf("Bytes per sector: %d\r\n", bpb->recsiz);
	printf("Test start sector: %d\r\n", bpb->datrec);
	printf("Test total sectors: %d\r\n", totsec);

	/* find the biggest number of sectors that can be read at once
	 * IOW the max number of sectors for which we can allocate memory for both the read and write buffers */
	unsigned char *buf = 0;
	long bufsz;
	short maxsec;
	/* 1st find the nearest power of 2 */
	for (maxsec = 1; maxsec * 2 < totsec; maxsec *= 2);
	/* then try to allocate the biggest available buffer */
	for (bufsz = maxsec * bpb->recsiz; maxsec > 1; maxsec /= 2, bufsz /= 2) {
		buf = malloc(2 * bufsz);
		if (buf)
			break;
	}

	if (!buf)
		quit(1, "Unable to allocate a minimum of %d bytes", bufsz);

	memset(buf, 0xae, bufsz);

	printf("Max sectors: %d\r\n", maxsec);
	printf("Write buffer: %p (%ld bytes)\r\n", buf, bufsz);
	printf("Read buffer: %p (%ld bytes)\r\n\n", buf + bufsz, bufsz);

	printf("Press any key to interrupt the test\r\n");
	printf("(on-going I/O must complete though).\r\n\n");

	long errcount = 0;
	for (short startsec = bpb->datrec; startsec <= bpb->datrec + totsec - maxsec; startsec += maxsec) {

		long bytes = bpb->recsiz;
		for (short nsec = 1; nsec <= maxsec; nsec *= 2, bytes *= 2) {

			check_user_interruption();

			print_full_line(0, "Writing %ldB %d sectors [%d,%d[", bytes, nsec, startsec, startsec + nsec);
			long err = Rwabs(1 /* write */, buf, nsec, startsec, dev);
			if (err) {
				fprintf(stderr, "\r\nWrite error %lx\r\n", err);
				errcount++;
			}

			check_user_interruption();

			print_full_line(0, "Reading %ldB %d sectors [%d,%d[", bytes, nsec, startsec, startsec + nsec);
			memset(buf + bufsz, 0x56, bytes);
			err = Rwabs(0 /* read */, buf + bufsz, nsec, startsec, dev);
			if (err) {
				fprintf(stderr, "\r\nRead error %lx\r\n", err);
				errcount++;
			}

			check_user_interruption();

			print_full_line(0, "Checking %ldB %d sectors [%d,%d[", bytes, nsec, startsec, startsec + nsec);
			if (!err && memcmp(buf, buf + bufsz, bytes)) {
				fprintf(stderr, "\r\nRead buffer does not match write buffer\r\n");
				for (long i=0; i<bytes; i++) {
					if (buf[i] != buf[bufsz+i]) {
						printf("[%d] %0.2x != %0.2x\r\n", i, buf[i], buf[bufsz+i]);
						break;
					}
				}
				errcount++;
			}
		}
	}

	print_full_line(0, "Test end. %ld errors detected.", errcount);
	quit(0, "");
}
