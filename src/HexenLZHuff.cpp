
#include <stdio.h>
#include <vector>
#include <conio.h>

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned short ushort;

/********** LZSS compression **********/

#define N		    4096	// buffer size
#define F		    128	    // lookahead buffer size
#define THRESHOLD	2
#define NIL		    N	    // leaf of tree

/* Huffman coding */
#define N_CHAR  	(256 - THRESHOLD + F)   // kinds of characters (character code = 0..N_CHAR-1) 382
#define T 		    (N_CHAR * 2 - 1)        // size of table 763
#define R 		    (T - 1)	                // position of root 762
#define MAX_FREQ	0x8000           // updates tree when the root frequency comes to this value.

byte* inputBuffer;  // 80076f44
byte* outputBuffer; // 80076f48
byte  text_buf[N + F - 1]; // 8007af50
short lson[N + 1]; // 8007bfd0
short rson[N + 257]; // 8007dfd8
short dad[N + 1]; // 800801e0
unsigned short freq[T + 1]; // 800821e8 
short prnt[T + N_CHAR]; // 800827e0
short son[T]; // 800830d8
short match_position; // 800836ce
short match_length; // 800836d0
int input_bit_count; // 800836d4
int output_bit_count; // 800836d8

// table for encoding and decoding the upper 6 bits of position

// for encoding
unsigned char p_len[64] = { // 800682a0
    0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

unsigned char p_code[64] = { // 800682e0
    0x00, 0x20, 0x30, 0x40, 0x50, 0x58, 0x60, 0x68,
    0x70, 0x78, 0x80, 0x88, 0x90, 0x94, 0x98, 0x9C,
    0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC,
    0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC, 0xCE,
    0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE,
    0xE0, 0xE2, 0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
    0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// for decoding
unsigned char d_code[256] = { // 80068320
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
    0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

unsigned char d_len[256] = { // 80068420
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

/*
========================
=
= _SwapInt
= A helper function to swap endianness for integers.
=
========================
*/

int _SwapInt(int value) {
    return ((value & 0x000000ff) << 24) | ((value & 0xff000000) >> 24) | ((value & 0x00ff0000) >> 8) | ((value & 0x0000ff00) << 8);
}

/*
========================
=
= WriteBinary
= Writes a single bit to the output buffer.
=
========================
*/

void WriteBinary(short bit) {  // 8004bb40
    if (bit == 0) {
        *outputBuffer = *outputBuffer & ~(byte)(1 << (output_bit_count & 0x1fU));
    }
    else {
        *outputBuffer = *outputBuffer | (byte)(1 << (output_bit_count & 0x1fU));
    }
    output_bit_count++;
    if (output_bit_count == 8) {
        output_bit_count = 0;
        outputBuffer++;
    }
}

/*
========================
=
= WriteCodeBinary
= Writes a specified number of bits from a code to the output buffer.
=
========================
*/

void WriteCodeBinary(int count, int binary) { // 8004bbd8
    for (int i = 0; i < count; i++) {
        uint shift = i & 0x1f;
        WriteBinary((binary & 1 << shift));
    }
}

/*
========================
=
= InitTree
= Initializes the binary tree used for compression.
=
========================
*/

void InitTree(void) { // 8004be40
    int  i;
    for (i = N + 1; i <= N + 256; i++) {
        rson[i] = NIL;			// root
    }
    for (i = 0; i < N; i++) {
        dad[i] = NIL;			// node
    }
}

/*
========================
=
= InsertNode
= Insert node to tree.
=
========================
*/

void InsertNode(short r) { // 8004be94
    int  i, p, cmp;
    unsigned char* key;
    unsigned c;

    cmp = 1;
    key = &text_buf[r];
    p = N + 1 + key[0];
    rson[r] = lson[r] = NIL;
    match_length = 0;
    for (; ; ) {
        if (cmp >= 0) {
            if (rson[p] != NIL) {
                p = rson[p];
            }
            else {
                rson[p] = r;
                dad[r] = p;
                return;
            }
        }
        else {
            if (lson[p] != NIL) {
                p = lson[p];
            }
            else {
                lson[p] = r;
                dad[r] = p;
                return;
            }
        }
        for (i = 1; i < F; i++) {
            if ((cmp = key[i] - text_buf[p + i]) != 0) {
                break;
            }
        }
        if (i > THRESHOLD) {
            if (i > match_length) {
                match_position = ((r - p) & (N - 1)) - 1;
                if ((match_length = i) >= F) {
                    break;
                }
            }
            if (i == match_length) {
                if ((c = ((r - p) & (N - 1)) - 1) < match_position) {
                    match_position = c;
                }
            }
        }
    }
    dad[r] = dad[p];
    lson[r] = lson[p];
    rson[r] = rson[p];
    dad[lson[p]] = r;
    dad[rson[p]] = r;
    if (rson[dad[p]] == p) {
        rson[dad[p]] = r;
    }
    else {
        lson[dad[p]] = r;
    }
    dad[p] = NIL;  // remove p
}

/*
========================
=
= DeleteNode
= Remove node from tree.
=
========================
*/

void DeleteNode(short p) { // 8004c0e4
    int  q;

    if (dad[p] == NIL) {
        return;			// not registered
    }
    if (rson[p] == NIL) {
        q = lson[p];
    }
    else {
        if (lson[p] == NIL) {
            q = rson[p];
        }
        else {
            q = lson[p];
            if (rson[q] != NIL) {
                do {
                    q = rson[q];
                } while (rson[q] != NIL);
                rson[dad[q]] = lson[q];
                dad[lson[q]] = dad[q];
                lson[q] = lson[p];
                dad[lson[p]] = q;
            }
            rson[q] = rson[p];
            dad[rson[p]] = q;
        }
    }
    dad[q] = dad[p];
    if (rson[dad[p]] == p) {
        rson[dad[p]] = q;
    }
    else {
        lson[dad[p]] = q;
    }
    dad[p] = NIL;
}

/*
========================
=
= StartHuff
= Initializes the Huffman coding tree.
=
========================
*/

void StartHuff(void) { // 8004c280
    int i, j;
    for (i = 0; i < N_CHAR; i++) {
        freq[i] = 1;
        son[i] = i + T;
        prnt[i + T] = i;
    }
    i = 0; j = N_CHAR;
    while (j <= R) {
        freq[j] = freq[i] + freq[i + 1];
        son[j] = i;
        prnt[i] = prnt[i + 1] = j;
        i += 2; j++;
    }
    freq[T] = 0xffff;
    prnt[R] = 0;
}

/*
========================
=
= Reconst
= Reconstructs the Huffman tree.
=
========================
*/

void Reconst(void) { // 8004c348
    int i, j, k;
    unsigned int f, l;

    // collect leaf nodes in the first half of the table
    // and replace the freq by (freq + 1) / 2.
    j = 0;
    for (i = 0; i < T; i++) {
        if (son[i] >= T) {
            freq[j] = (freq[i] + 1) / 2;
            son[j] = son[i];
            j++;
        }
    }

    // begin constructing tree by connecting sons
    for (i = 0, j = N_CHAR; j < T; i += 2, j++) {
        k = i + 1;
        f = freq[j] = freq[i] + freq[k];
        for (k = j - 1; f < freq[k]; k--);
        k++;
        l = ((j - k) * 2) & 0xffff;
        memmove(&freq[k + 1], &freq[k], l);
        freq[k] = f;
        memmove(&son[k + 1], &son[k], l);
        son[k] = i;
    }

    // connect prnt
    for (i = 0; i < T; i++) {
        if ((k = son[i]) >= T) {
            prnt[k] = i;
        }
        else {
            prnt[k] = prnt[k + 1] = i;
        }
    }
}

/*
========================
=
= Update
= Increment frequency of given code by one, and Update tree
=
========================
*/

void Update(int c) { // 8004c588
    int i, j, k, l;

    if (freq[R] == MAX_FREQ) {
        Reconst();
    }
    c = prnt[c + T];
    do {
        k = ++freq[c];

        // if the order is disturbed, exchange nodes
        if (k > freq[l = c + 1]) {
            while (k > freq[l]) {
                l++;
            }

            l--;
            freq[c] = freq[l];
            freq[l] = k;

            i = son[c];
            prnt[i] = l;
            if (i < T) {
                prnt[i + 1] = l;
            }

            j = son[l];
            son[l] = i;

            prnt[j] = c;
            if (j < T) {
                prnt[j + 1] = c;
            }
            son[c] = j;

            c = l;
        }
    } while ((c = prnt[c]) != 0);	// repeat up to root
}

/*
========================
=
= EncodeChar
= Encode single char data
=
========================
*/

void EncodeChar(uint c) { // 8004c71c
    unsigned i;
    int j, k;

    i = 0;
    j = 0;
    k = prnt[c + T];

    // travel from leaf to root
    do {
        // if node's address is odd-numbered, choose bigger brother node
        i = (i & 0x7fff) << 1 | k & 1;

        j++;
    } while ((k = prnt[k]) != R);

    // write the i code with its bit j
    WriteCodeBinary(j, i);

    // Update frequency tables
    Update(c);
}

/*
========================
=
= EncodePosition
= Encode position offset.
=
========================
*/

void EncodePosition(uint c) { // 8004c7ac
    unsigned  i, j;

    i = c >> 6;
    j = p_len[i];

    // output upper  bits by table lookup
    WriteCodeBinary(8, (((uint)p_code[i] << 8) + ((c & 0x3f) << (-(uint)j + 10 & 0x1f)) & 0xffff) >> 8);
    // output lower bits verbatim
    WriteCodeBinary(6 - (-(uint)j + 8), c & 0x3f);
}

/*
========================
=
= EncodeEnd
= Handles the end of encoding.
=
========================
*/

void EncodeEnd(void) { // 8004c83c
    // Check if there are remaining bits in the output bit buffer
    if (output_bit_count != 0) {
        // Write zeros to the output until the bit buffer is empty
        while (output_bit_count != 0) {
            WriteBinary(0);
        }
    }
}

/*
========================
=
= DecodeChar
= Decode single char data
=
========================
*/

int DecodeChar(void) { // 8004cb78
    unsigned c;

    c = son[R];

    // travel from root to leaf,
    // choosing the smaller child node (son[]) if the read bit is 0,
    // the bigger (son[]+1} if 1
    while (c < T) {
        if ((1 << (input_bit_count & 0x1fU) & (inputBuffer[2] << 16) + (inputBuffer[1] << 8) + inputBuffer[0]) != 0) {
            c++;
        }
        input_bit_count++;
        c = son[c];
    }
    inputBuffer += (input_bit_count >> 3);
    input_bit_count &= 7;
    c -= T;
    Update(c);
    return c;
}

/*
========================
=
= DecodePosition
= Decode position offset.
=
========================
*/

int DecodePosition(void) { // 8004cc58
    uint c, h, i, j, s;

    // recover upper bits from table
    h = inputBuffer[1];
    i = (int)(inputBuffer[0] + (h << 8)) >> (input_bit_count & 0x1fU) & 0xff;

    c = (unsigned)d_code[i] << 6;

    // read lower bits verbatim
    j = d_len[i] - 2;
    s = input_bit_count & 0x1f;

    // Update input_bit_count and inputBuffer
    input_bit_count += (j & 0xffff);
    if (input_bit_count > 7) {
        input_bit_count -= 8;
        inputBuffer = inputBuffer + 2;
    }
    else {
        inputBuffer = inputBuffer + 1;
    }

    // Calculate the result based on the decoded values
    return (int)((uint)c | ((i << (j & 0x1f)) + ((int)(h + (inputBuffer[0] << 8) & 0xffff) >> s & (1 << (j & 0x1f)) - 1U & 0xffff) & 0x3f));
}

/*
========================
=
= Encode
= Compression function
=
========================
*/

int Encode(byte* input, byte* output, int size) // 8004cd28
{
    int  i, c, len, r, s, last_match_length;

    output_bit_count = 0;
    input_bit_count = 0;
    inputBuffer = input;
    outputBuffer = output;
    *(int*)output = _SwapInt(size);
    outputBuffer += sizeof(int);

    if (size == 0) {
        return 4;
    }

    StartHuff();
    InitTree();
    s = 0;
    r = (N - F);
    for (i = s; i < r; i++) {
        text_buf[i] = ' ';
    }
    for (len = 0; len < F && size != 0; len++, size--) {
        c = *inputBuffer++;
        text_buf[r + len] = c;
    }
    for (i = 1; i <= F; i++) {
        InsertNode(r - i);
    }
    InsertNode(r);
    do {
        if (match_length > len)
            match_length = len;
        if (match_length <= THRESHOLD) {
            match_length = 1;
            EncodeChar(text_buf[r]);
        }
        else {
            EncodeChar(255 - THRESHOLD + match_length);
            EncodePosition(match_position);
        }
        last_match_length = match_length;
        for (i = 0; i < last_match_length && size != 0; i++, size--) {
            c = *inputBuffer++;
            DeleteNode(s);
            text_buf[s] = c;
            if (s < F - 1)
                text_buf[s + N] = c;
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            InsertNode(r);
        }

        while (i++ < last_match_length) {
            DeleteNode(s);
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);
            if (--len) InsertNode(r);
        }
    } while (len > 0);
    EncodeEnd();

    return (int)(outputBuffer - output);
}

/*
========================
=
= Decode
= Decompression function
=
========================
*/

int Decode(byte* input, byte* output, int length) { // 8004d23c
    int  i, j, k, r, c;
    unsigned long int  count;

    // Initialize the decoding parameters and buffers
    output_bit_count = 0;
    input_bit_count = 0;
    inputBuffer = input;
    outputBuffer = output;

    if (length != 0) {
        StartHuff();
        for (i = 0; i < N - F; i++) {
            text_buf[i] = ' ';
        }
        r = N - F;
        for (count = 0; count < length; ) {
            c = DecodeChar();
            if (c < 256) {
                *outputBuffer++ = c;
                text_buf[r++] = c;
                r &= (N - 1);
                count++;
            }
            else {
                i = (r - DecodePosition() - 1) & (N - 1);
                j = c - 255 + THRESHOLD;
                for (k = 0; k < j; k++) {
                    c = text_buf[(i + k) & (N - 1)];
                    *outputBuffer++ = c;
                    text_buf[r++] = c;
                    r &= (N - 1);
                    count++;
                }
            }
        }
    }

    return length;
}
