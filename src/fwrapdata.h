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

#ifndef _FWRAPDATA_H_
#define _FWRAPDATA_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Provide stream access to a CFMutableData structure.
 *
 * It is possible to access the underlying data concurrently with stdio
 * functions, CFData/CFMutableData functions, and NSData/NSMutableData
 * methods. Direct access to the underlying data does not change the stream
 * position (it may be desirable to fseek() after adding or changing bytes).
 *
 * fclose() must eventually be called on the returned handle to liberate
 * internally allocated memory.
 */
FILE *fwrapdata(CFMutableDataRef data);

#ifdef __cplusplus
}
#endif

#endif

