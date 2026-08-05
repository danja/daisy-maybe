/* dirac_fmt.h — minimal integer/string formatting for the display.
 * SPDX-License-Identifier: Apache-2.0
 *
 * snprintf drags in newlib's nano-vfprintf, which in turn pulls malloc,
 * realloc and the FILE machinery — several KB of a 128 KB flash budget that
 * this firmware does not have to spare. Everything shown on a 10-column
 * display is an integer, a fixed string, or a two-part fixed-point value, so
 * these few helpers replace it outright. */

#pragma once

namespace dirac
{
namespace fmt
{

/* Writes a signed integer, returns the number of characters written. */
inline int Int(char *dst, int len, int v)
{
    if(len <= 0)
        return 0;
    char tmp[12];
    int n = 0;
    unsigned u = (v < 0) ? unsigned(-(long long)v) : unsigned(v);
    do
    {
        tmp[n++] = char('0' + (u % 10u));
        u /= 10u;
    } while(u && n < int(sizeof(tmp)));
    if(v < 0 && n < int(sizeof(tmp)))
        tmp[n++] = '-';
    int w = 0;
    while(n > 0 && w < len - 1)
        dst[w++] = tmp[--n];
    dst[w] = '\0';
    return w;
}

/* Copies a string, truncating to fit. Returns characters written. */
inline int Str(char *dst, int len, const char *s)
{
    int w = 0;
    while(s && *s && w < len - 1)
        dst[w++] = *s++;
    if(len > 0)
        dst[w] = '\0';
    return w;
}

inline int Len(const char *s)
{
    int n = 0;
    while(s && s[n])
        n++;
    return n;
}

/* "-3.7" style: whole and fractional parts of a value already scaled by 10. */
inline void Dec1(char *dst, int len, int tenths)
{
    const bool neg = tenths < 0;
    const int a = neg ? -tenths : tenths;
    int w = 0;
    if(neg && w < len - 1)
        dst[w++] = '-';
    w += Int(dst + w, len - w, a / 10);
    if(w < len - 1)
        dst[w++] = '.';
    if(w < len - 1)
        dst[w++] = char('0' + (a % 10));
    if(len > 0)
        dst[w] = '\0';
}

/* Writes `s` into a fixed-width field, padding with spaces. Does NOT
 * null-terminate — the caller is assembling a fixed-layout line. */
inline void Field(char *dst, int width, const char *s, bool rightAlign)
{
    int n = Len(s);
    if(n > width)
        n = width;
    const int pad = width - n;
    int w = 0;
    if(rightAlign)
        for(int i = 0; i < pad; ++i)
            dst[w++] = ' ';
    for(int i = 0; i < n; ++i)
        dst[w++] = s[i];
    if(!rightAlign)
        for(int i = 0; i < pad; ++i)
            dst[w++] = ' ';
}

} // namespace fmt
} // namespace dirac
