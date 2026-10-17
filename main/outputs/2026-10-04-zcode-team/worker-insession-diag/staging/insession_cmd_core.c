/* insession_cmd_core.c — strict grammar-v1 scanner for the in-session intake.
   worker-insession-diag, 2026-10-05. See insession_cmd_core.h for the grammar.

   Implementation law (diagnostic.c discipline): fixed buffers, no allocation,
   no I/O, no recursion; every input byte is classified before use. The only
   libc calls are strcmp/strlen — both already strong UND members of the
   composed candidate's 196-import contract (supplier-contract.json,
   required_strong=196, missing_strong=0), so they add no new import. */

#include "insession_cmd_core.h"

#include <string.h>

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int is_tag_char(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && is_ws(*p))
        ++p;
    return p;
}

/* Returns one past c, or NULL when c is not at p. */
static const char *match_char(const char *p, const char *end, char c)
{
    return (p < end && *p == c) ? p + 1 : NULL;
}

/* Parses a JSON string with NO escape support into out (capacity includes the
   NUL). Rejects backslash escapes, control bytes, and everything >= 0x7f.
   Returns one past the closing quote, or NULL. */
static const char *parse_string(const char *p, const char *end,
                                char *out, size_t capacity)
{
    size_t used = 0;
    p = match_char(p, end, '"');
    if (!p)
        return NULL;
    while (p < end && *p != '"')
    {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c < 0x20 || c >= 0x7f)
            return NULL; /* escapes/non-printable are rejected in v1 */
        if (used + 1 >= capacity)
            return NULL; /* longer than this field allows */
        out[used++] = (char)c;
        ++p;
    }
    p = match_char(p, end, '"');
    if (!p)
        return NULL;
    out[used] = '\0';
    return p;
}

/* Parses unsigned decimal digits only: no sign, no leading zero ("0" itself
   allowed and range-checked by the caller), at most 4 digits (anything longer
   cannot be <= INSESSION_REPEAT_MAX anyway — bounded). */
static const char *parse_uint(const char *p, const char *end, uint32_t *out)
{
    uint32_t value = 0;
    int digits = 0;
    char first;
    if (p >= end || !is_digit(*p))
        return NULL;
    first = *p;
    while (p < end && is_digit(*p))
    {
        if (++digits > 4)
            return NULL;
        value = value * 10u + (uint32_t)(*p - '0');
        ++p;
    }
    if (digits > 1 && first == '0')
        return NULL; /* strict JSON: no leading zeros */
    *out = value;
    return p;
}

void insession_parse_v1(const char *buf, size_t n, struct insession_cmd *out)
{
    const char *end = buf + n;
    const char *p = buf;
    char key[24];          /* every legal key is far shorter; longer -> unknown */
    char cmd_value[16];    /* every legal cmd value is shorter; longer -> unknown */
    int seen_cmd = 0, seen_tag = 0, seen_repeat = 0;

    out->verdict = INSESSION_REJECT;
    out->tag[0] = '\0';
    out->repeat = 0;
    out->reason = "malformed";

    p = skip_ws(p, end);
    if (!(p = match_char(p, end, '{')))
        return;
    cmd_value[0] = '\0';
    p = skip_ws(p, end);
    if (p < end && *p == '}')
    {
        ++p; /* empty object: falls through to "missing cmd" below */
    }
    else
    {
        for (;;)
        {
            p = skip_ws(p, end);
            p = parse_string(p, end, key, sizeof(key));
            if (!p)
                return;
            p = skip_ws(p, end);
            if (!(p = match_char(p, end, ':')))
                return;
            p = skip_ws(p, end);
            if (strcmp(key, "cmd") == 0)
            {
                if (seen_cmd)
                {
                    out->reason = "duplicate key";
                    return;
                }
                seen_cmd = 1;
                p = parse_string(p, end, cmd_value, sizeof(cmd_value));
            }
            else if (strcmp(key, "tag") == 0)
            {
                if (seen_tag)
                {
                    out->reason = "duplicate key";
                    return;
                }
                seen_tag = 1;
                p = parse_string(p, end, out->tag, sizeof(out->tag));
            }
            else if (strcmp(key, "repeat") == 0)
            {
                uint32_t value;
                if (seen_repeat)
                {
                    out->reason = "duplicate key";
                    return;
                }
                seen_repeat = 1;
                p = parse_uint(p, end, &value);
                if (p)
                    out->repeat = value;
            }
            else
            {
                out->reason = "unknown key"; /* never silently skipped in v1 */
                return;
            }
            if (!p)
                return;
            p = skip_ws(p, end);
            if (p < end && *p == ',')
            {
                ++p;
                continue;
            }
            if (p < end && *p == '}')
            {
                ++p;
                break;
            }
            return; /* malformed separator or terminator */
        }
    }
    if (skip_ws(p, end) != end)
    {
        out->reason = "trailing bytes";
        return;
    }

    if (!seen_cmd)
    {
        out->reason = "missing cmd";
        return;
    }
    if (strcmp(cmd_value, "noop") == 0)
    {
        if (seen_tag || seen_repeat)
        {
            out->reason = "noop with extra keys";
            return;
        }
        out->verdict = INSESSION_NOOP;
        out->reason = NULL;
        return;
    }
    if (strcmp(cmd_value, "diag") == 0)
    {
        size_t i, tlen;
        if (!seen_tag || !seen_repeat)
        {
            out->reason = "diag needs tag+repeat";
            return;
        }
        if (out->repeat < 1u || out->repeat > (uint32_t)INSESSION_REPEAT_MAX)
        {
            out->reason = "repeat out of range";
            return;
        }
        tlen = strlen(out->tag);
        if (tlen == 0)
        {
            out->reason = "empty tag";
            return;
        }
        for (i = 0; i < tlen; ++i)
        {
            if (!is_tag_char((unsigned char)out->tag[i]))
            {
                out->reason = "bad tag charset";
                return;
            }
        }
        out->verdict = INSESSION_DIAG;
        out->reason = NULL;
        return;
    }
    out->reason = "unknown cmd"; /* e.g. "reload": v1 NEVER executes it */
}
