#include <stdio.h>

/*
 * rtmodem.c -- This program allows the RT-11 user to transfer files to
 * and from a host system (usually CP/M) using Ward Christensen's
 * MODEM/XMODEM protocol.  No terminal emulation is provided (yet).
 * Written in DECUS C by:
 *
 * Robert B. Hoffman, N3CVL	5-Mar-82
 * <Hoffman%Pitt.csnet@CSNet-Relay.ARPA>
 * {allegra, bellcore, cadre, idis, psuvax1}!pitt!hoffman
 *
 * 5-Mar-82	Original Version
 *
 * 18-Mar-83	Changed transmit routine to wait for one of (ACK,NAK,CAN,
 *		or TIMEOUT) before re-sending the sector.  Garbage chars are
 *		now ignored.  Changed receive routine to print a count of
 *		bytes transferred.
 *
 * 26-Aug-83	Changed transmit routine to print a count of bytes
 *		transferred.  Changed receive routine to cancel transfer if
 *		the allocated file size is exceeded. *** Note:  This is
 *		bogus -- putc() isn't returning any error condition. (see below)
 *
 * 9-Dec-83	Changed receive routine to send NAKs only after it times
 *		out waiting for a SOH, rather than NAKing every non-SOH.
 *
 * 13-Feb-84	Changed buffer array from char to ints.  It seems that when
 *		putc was called with a char with the eighth bit set, then
 *		it was returning a negative int, which appeared as an error.
 *		If putc is called with an int, it works just fine, and returns
 *		the expected error condition not found above.  Also changed
 *		command structure to make it quicker.  Now, when a transfer
 *		completes, it returns to command mode rather than exiting.
 *
 * 19-Feb-84	Changed the 'bytes transmitted' message to indicate sectors,
 *		blocks, and bytes.  Added bell chars to termination messages.
 *
 * 15-Aug-84	Fixed a bug that allowed nulls to be transmitted in text mode
 *		and appended an extra sector's worth of nulls in binary mode.
 *		Also changed command scanner to add Ethernet escape option
 *		and to make it easier to add more options.  Added Ethernet
 *		escapes to output driver.
 */

struct device {
	int rcsr;
	int rbuf;
	int xcsr;
	int xbuf;
};

#define SOH	01
#define EOT	04
#define ACK	06
#define NAK	025
#define CAN	030
#define TIMEOUT	-1
#define TRUE	1
#define FALSE	0
#define EVER	;;

int $$narg = 1;

struct device *dladdr = 0176500;

int buf[128];
int ether;
FILE *file;

main() {
	register int i, c;
	register char *p;
	char blkno, cblkno, *filnam, cmdlin[40];
	int bcnt, cksm, ok, nakcount, sndrcv, txtbin;
	long nbytes;

	printf("RTMODEM -- RT-11 file transfer program\n\n");

cmd:
	do {
		printf("RTMODEM command=> ");
		gets(cmdlin);
		for (i=0; i<strlen(cmdlin); i++)
			cmdlin[i] = (isupper(cmdlin[i]) ? tolower(cmdlin[i]) : cmdlin[i]);
		filnam = index(cmdlin,' ');
		*filnam++ = 0;	/* this is sneaky -- it null-terminates the */
				/* command part of the string and makes */
				/* filnam point to the file name */

		ok = TRUE;		/* assume cmd will be OK */
		p = index(cmdlin,'s');	/* sending */
		if (p == NULL) {
			p = index(cmdlin,'r');	/* receiving */
		}
		if (p == NULL) {
			ok = FALSE;	/* one of these MUST be present */
		}
		sndrcv = *p;

		p = index(cmdlin,'t');	/* text */
		if (p == NULL) {
			p = index(cmdlin,'b');	/* binary */
		}
		if (p == NULL) {
			ok = FALSE;	/* one of these must also be present */
		}
		txtbin = *p;

		if (index(cmdlin,'e') == NULL) {	/* ethernet mode */
			ether = FALSE;
		} else {
			ether = TRUE;
		}

		if (!ok) {
			printf("\nCommands are of the form:\n");
			printf("   rt|rb|st|sb [e]   filename\n");
			printf("      rt -- receive text\n");
			printf("      rb -- receive binary\n");
			printf("      st -- send text\n");
			printf("      sb -- send binary\n");
			printf("      e  -- use Ethernet escape\n\n");
		}
	} while (!ok);

	freopen("tt:", "wu", stdout);
	if (ether) {
		printf("Using Ethernet character escape.\n");
	}
	if (sndrcv == 's') {
		printf("Sending file %s, %s mode.\n", filnam,
			(txtbin == 't' ? "text" : "binary"));
		if ((file = fopen(filnam,"rn")) == NULL) {
			fprintf(stderr,"Couldn't open input file %s.\n",
				filnam);
			goto cmd;
		}
		printf("File open, ready to send.\n");
		nbytes = 512L * (long)(file->io_size);
		printf("File is %d sectors (%ld bytes) long.\n",
			(file->io_size) * 4, nbytes);
		nbytes /= 84L;	/* 120 cps * 70% = 84 cps effective */
		printf("Estimated transfer time:  %ld min, %ld sec at 1200 bps.\n",
				nbytes/60L, nbytes%60L);

		while ((c = getmod()) != NAK)
			;

		nbytes = 0L;
		blkno = 0;
		do {
			i = 0;
			do {
				c = getc(file);
				if (c == EOF) {
					if (i == 0)
						goto sfin;
					if (txtbin == 't')
						buf[i++] = 032;
					else
						buf[i++] = 0;
				}
				else if (txtbin != 't' || c != 0) {
					buf[i++] = c;
				}
			} while (i < 128);
			blkno++;
			do {
				outmod(SOH);
				outmod(blkno);
				outmod(~blkno);
				cksm = 0;
				for (bcnt = 0; bcnt < 128; bcnt++) {
					outmod(buf[bcnt]);
					cksm = (cksm + buf[bcnt]) % 256;
				}
				outmod(cksm);
				do {
					ok = TRUE;	/* Assume all OK */
					c = getmod();
					switch (c) {
					case ACK:
					case NAK:
					case CAN:
					case TIMEOUT:
						break;

					default:
						ok = FALSE;
						break;
					}
				} while (!ok);
			} while (c != ACK && c != CAN);
	
			nbytes += 128L;
			printf("%ld sec, %ld blk, %ld b\r", nbytes/128L,
				(nbytes+384L)/512L, nbytes);

			if (c == CAN) {
				fclose(file);
				printf("\n\007\007\007Transfer cancelled by host.\n");
				goto cmd;
			}
	
		} while (c == ACK && !feof(file));
	
sfin:
		outmod(EOT);
		c = getmod();
		if (c == ACK)
			printf("\n\007\007\007File transmitted\n");
		else
			printf("\n\007\007\007Final ACK was 0%o instead!\n",c);
	
		fclose(file);
		goto cmd;
	}
	else {
		printf("Receiving file %s, %s mode.\n", filnam,
			(txtbin == 't' ? "text" : "binary"));
		if ((file = fopen(filnam,"wn")) == NULL) {
			fprintf(stderr,"Couldn't open output file %s\n",
				filnam);
			goto cmd;
		}
		printf("File open, ready to receive.\n");

		nbytes = 0L;
		outmod(NAK);	/* Send initial NAK */
		nakcount = 0;
		for (EVER) {
			do {
				c = getmod();
				switch (c) {
				case TIMEOUT:
					outmod(NAK);
					nakcount++;
					break;

				case EOT:
					goto rfin;

				default:
					break;
				}
			} while (c != SOH);

			blkno = getmod();
			cblkno = getmod();
			cksm = 0;
			for (i=0; i<128; i++) {
				c = buf[i] = getmod();
				cksm = (cksm + c) % 256;
			}
			c = getmod();
			ok = (((SOH+blkno+cblkno) % 256) == 0 && cksm == c);
			if (ok) {
				for (i=0; i<128; i++) {
					if (buf[i] == '\032' && txtbin == 't')
						break;
					c = putc(buf[i],file);
					if (c == EOF) {
						outmod(CAN);
						fclose(file);
						printf("\n\07Disk full!  Transfer cancelled.\n");
						goto cmd;
					}
				}
				nbytes += 128L;
				printf("%ld sec, %ld blk, %ld b\r",
					nbytes/128L, (nbytes+384L)/512L,
					nbytes);
				outmod(ACK);
				nakcount = 0;
			} else {
				if (nakcount > 10) {
					printf("\nR)etry or Q)uit=> ");
					gets(cmdlin);
					c = cmdlin[0];
					if (c == 'q' || c == 'Q') {
						outmod(CAN);
						fclose(file);
						goto cmd;
					}
					nakcount = 0;
				}
				nakcount++;
				printf("\nNAK %d\n", nakcount);
				outmod(NAK);
			}
		}
rfin:
		fclose(file);
		outmod(ACK);
		printf("\n\007\007\007Transfer complete\n");
		goto cmd;
	}
}

/*
 * get a character from the modem.   A 10 second timeout is
 * needed to preclude hanging the modem if one side should
 * terminate abnormally.  The constant of 160000 provides this
 * delay for an LSI-11.
 */

getmod() {
	register struct device *MODEM;
	long n;

	MODEM = dladdr;
	for (n=160000L; ((MODEM->rcsr & 0200) == 0) && (n > 0L); n--)
		;
	if (n == 0L) {
		printf("\nTimeout!\n");
		return(-1);
	}
	return((MODEM->rbuf) & 0377);
}

/*
 * Send a character to the modem.
 */

outmod(c)
char c;
{
	if (ether) {
		switch(c) {
		case 020:
		case 021:
		case 023:
			outm(020);
			break;
		default:
			break;
		}
	}
	outm(c);
}

outm(c)
char c;
{
	register struct device *MODEM;

	MODEM = dladdr;
	while ((MODEM->xcsr & 0200) == 0)
		;
	MODEM->xbuf = c;
}
