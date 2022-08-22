#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <ext.h>
#include <mint/osbind.h>

static long errcount;
static long pass;

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
		print_full_line(1, "Interrupted. Pass %ld, %ld errors.", pass, errcount);
		quit(1, "");
	}
}

static void print_progress(long pass, const char *op, short startsec, short nsec, long bytes)
{
	print_full_line(0, "Pass %ld %s [%d,%d[ %d sec %ldB", pass, op, startsec, startsec + nsec, nsec, bytes);
}

int main(int argc, char **argv)
{
	char drive, dev;

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
	printf("Floppy %c, %d sec %dB/sec\r\n", drive, totsec + bpb->datrec, bpb->recsiz);

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

	short startsec = bpb->datrec;
	short endsec = bpb->datrec + totsec - totsec % maxsec;

	printf("Test sec [%d,%d[, max IO %d sec\r\n\n", startsec, endsec, maxsec);

	printf("Press any key to interrupt the test\r\n");
	printf("(on-going IO must complete though).\r\n\n");

	for (pass = 1; pass < LONG_MAX; pass++) {
		for (short cursec = startsec; cursec < endsec; cursec += maxsec) {
			long bytes = bpb->recsiz;
			for (short nsec = 1; nsec <= maxsec; nsec *= 2, bytes *= 2) {

				check_user_interruption();

				/* use a different write pattern each time */
				char pattern = rand();
				/* write buffer is initialized with the pattern */
				memset(buf, pattern, bytes);
				/* read buffer is initialized with the opposite */
				memset(buf+bufsz, ~pattern, bytes);

				check_user_interruption();

				print_progress(pass, "Wr ", cursec, nsec, bytes);
				long err = Rwabs(3 /* write */, buf, nsec, cursec, dev);
				if (err) {
					fprintf(stderr, "\r\nWrite error (%ld)\r\n", err);
					errcount++;
				}

				check_user_interruption();

				print_progress(pass, "Rd ", cursec, nsec, bytes);
				err = Rwabs(2 /* read */, buf + bufsz, nsec, cursec, dev);
				if (err) {
					fprintf(stderr, "\r\nRead error (%ld)\r\n", err);
					errcount++;
				}

				check_user_interruption();

				print_progress(pass, "Chk", cursec, nsec, bytes);
				if (!err && memcmp(buf, buf + bufsz, bytes)) {
					fprintf(stderr, "\r\nRead buffer does not match write buffer\r\n");
					for (long i=0; i<bytes; i++) {
						if (buf[i] != buf[bufsz+i]) {
							printf("1st error @%d Wr %0.2x != Rd %0.2x\r\n", i, buf[i], buf[bufsz+i]);
							break;
						}
					}
					errcount++;
				}
			}
		}
	}

	print_full_line(0, "Test end. Pass %ld, %ld errors.", pass, errcount);
	quit(0, "");
}
