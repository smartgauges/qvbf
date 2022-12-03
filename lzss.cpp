/* LZSS encoder-decoder (Haruhiko Okumura; public domain) */

#include <stdio.h>
#include "lzss.h"

#define EI 10  /* typically 10..13 */
#define EJ  4  /* typically 4..5 */
#define P   1  /* If match length <= P then output one character */
#define N (1 << EI)  /* buffer size */
#define F ((1 << EJ) + P)  /* lookahead buffer size */

int bit_buffer = 0, bit_mask = 128;
unsigned char buffer[N * 2];

void putbit1(QByteArray & data)
{
	bit_buffer |= bit_mask;
	if ((bit_mask >>= 1) == 0) {

		data.append(bit_buffer);

		bit_buffer = 0;
		bit_mask = 128;
	}
}

void putbit0(QByteArray & data)
{
	if ((bit_mask >>= 1) == 0) {

		data.append(bit_buffer);

		bit_buffer = 0;
		bit_mask = 128;
	}
}

void flush_bit_buffer(QByteArray & data)
{
	if (bit_mask != 128) {

		data.append(bit_buffer);
	}
}

void output1(QByteArray & data, int c)
{
	int mask;

	putbit1(data);
	mask = 256;
	while (mask >>= 1) {
		if (c & mask)
			putbit1(data);
		else
			putbit0(data);
	}
}

void output2(QByteArray & data, int x, int y)
{
	int mask;

	putbit0(data);
	mask = N;
	while (mask >>= 1) {
		if (x & mask)
			putbit1(data);
		else
			putbit0(data);
	}
	mask = (1 << EJ);
	while (mask >>= 1) {
		if (y & mask)
			putbit1(data);
		else
			putbit0(data);
	}
}

qint32 data_idx = 0;

QByteArray encode(const QByteArray & data)
{
	QByteArray cdata;
	cdata.reserve(data.size());

	data_idx = 0;
	bit_buffer = 0;
	bit_mask = 128;

	int i, j, f1, x, y, r, s, bufferend, c;

	for (i = 0; i < N - F; i++)
		buffer[i] = '\0';

	for (i = N - F; i < N * 2; i++) {

		if (data_idx >= data.size())
			break;

		c = data[data_idx++];
		buffer[i] = c;
	}
	bufferend = i;
	r = N - F;
	s = 0;

	while (r < bufferend) {

		f1 = (F <= bufferend - r) ? F : bufferend - r;
		x = 0;
		y = 1;
		c = buffer[r];

		for (i = r - 1; i > s; i--) {

			if ((s >= (r - i)) && buffer[i] == c) {

				for (j = 1; j < f1; j++)
					if (buffer[i + j] != buffer[r + j])
						break;
				if (j > y) {
					x = i;
					y = j;
				}
			}
		}

		if (x >= (N - F))
			x -= (N - F);
		x++;

		if (y <= P)
			output1(cdata, c);
		else
			output2(cdata, x & (N - 1), y - 2);

		r += y;
		s += y;

		if (r >= N * 2 - F) {

			for (i = 0; i < N; i++)
				buffer[i] = buffer[i + N];

			bufferend -= N;
			r -= N;
			s -= N;

			while (bufferend < N * 2) {

				if (data_idx >= data.size())
					break;

				c = data[data_idx++];

				buffer[bufferend++] = c;
			}
		}
	}

	flush_bit_buffer(cdata);

	return cdata;
}

/* get n bits */
static int buf, mask = 0;
int getbit(const QByteArray & data, int n)
{
	int i, x;

	x = 0;
	for (i = 0; i < n; i++) {

		if (mask == 0) {

			if (data_idx >= data.size())
				return EOF;

			buf = data[data_idx++];
			mask = 128;
		}
		x <<= 1;
		if (buf & mask)
			x++;
		mask >>= 1;
	}

	return x;
}

QByteArray decode(const QByteArray & cdata)
{
	data_idx = 0;

	QByteArray data;
	if (cdata.size() < 40 * 1024 * 1024)
		data.reserve(cdata.size()*6);
	else
		data.reserve(cdata.size()*2);

	int i, j, k, r, c;

	//prepare global variable
	mask = 0;

	for (i = 0; i < N - F; i++)
		buffer[i] = ' ';

	//r = N - F;
	r = 0;
	while ((c = getbit(cdata, 1)) != EOF) {

		if (c) {
			if ((c = getbit(cdata, 8)) == EOF)
				break;

			//printf("False 0x%x\n", c);

			data.append(c);
			buffer[r++] = c;
			r &= (N - 1);
		} else {
			if ((i = getbit(cdata, EI)) == EOF)
				break;
			if ((j = getbit(cdata, EJ)) == EOF)
				break;

			if (i == 0)
				break;

			i -= 1;

			//printf("True (%u, %u)\n", i, j + 2);

			for (k = 0; k <= j + 1; k++) {
				c = buffer[(i + k) & (N - 1)];
				data.append(c);
				buffer[r++] = c;
				r &= (N - 1);
			}
		}
	}

	return data;
}

