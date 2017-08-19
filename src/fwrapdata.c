/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any means.

 * In jurisdictions that recognize copyright laws, the author or authors of
 * this software dedicate any and all copyright interest in the software to
 * the public domain. We make this dedication for the benefit of the public
 * at large and to the detriment of our heirs and successors. We intend this
 * dedication to be an overt act of relinquishment in perpetuity of all
 * present and future rights to this software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * 2017 Timothy Bourke (tim@tbrk.org)
 */

#include "fwrapdata.h"

typedef struct fstate {
    CFMutableDataRef data;
    CFIndex pos;
} fstate_t;

static int readfn(void *handler, char *buf, int size)
{
    fstate_t *state = handler;
    CFIndex len = CFDataGetLength(state->data);
    int available = (int)(len - state->pos);

    if (size > available)
	size = available;

    CFDataGetBytes(state->data,
		   CFRangeMake(state->pos, (CFIndex)size),
		   (UInt8 *)buf);

    state->pos += size;

    return size;
}

static int writefn(void *handler, const char *buf, int size)
{
    fstate_t *state = handler;
    CFIndex len = CFDataGetLength(state->data);
    CFIndex rlen;

    if (state->pos + (CFIndex)size > len) {
	rlen = len - state->pos;
    } else {
	rlen = (CFIndex)size;
    }

    CFDataReplaceBytes(state->data,
		       CFRangeMake(state->pos, rlen),
		       (UInt8 *)buf, (CFIndex)size);

    state->pos += size;

    return size;
}

static fpos_t seekfn(void *handler, fpos_t offset, int whence)
{
    CFIndex pos;
    fstate_t *state = handler;
    CFIndex len = CFDataGetLength(state->data);

    switch (whence) {
    case SEEK_SET:
	pos = (CFIndex)offset;
	break;

    case SEEK_CUR:
	pos = state->pos + (CFIndex)offset;
	break;

    case SEEK_END:
	pos = len + (CFIndex)offset;
	break;

    default:
	return -1;
    }

    if (pos > len)
	CFDataIncreaseLength(state->data, pos - len);

    state->pos = pos;
    return (fpos_t)pos;
}

static int closefn(void *handler)
{
    fstate_t *state = handler;
    CFRelease(state->data);
    free(handler);
    return 0;
}

FILE *fwrapdata(CFMutableDataRef data)
{
    fstate_t* state = (fstate_t *)malloc(sizeof(fstate_t));
    memset(state, 0, sizeof(fstate_t));

    state->data = data;
    CFRetain(state->data);

    return funopen(state, readfn, writefn, seekfn, closefn);
}

