#include "text.h"

#include <string.h>

const char* trim_front(const char* str)
{
    // Skip leading whitespace
    while (*str == ' ')
    {
        str++;
    }
    return str;
}

void trim_back(char* str)
{
    // Skip trailing whitespace by replacing it with '\0' characters
    int idx = strlen(str) - 1;
    while (idx >= 0 && str[idx] == ' ')
    {
        str[idx--] = '\0';
    }
}

char* trim(char* str)
{
    str = (char*)trim_front(str);
    trim_back(str);
    // return a pointer to the first non-whitespace character
    return str;
}

void trim_copy(char* trimmed, const char* str)
{
    str = (char*)trim_front(str);
    int len = strlen(str) - 1;
    while (len >= 0 && str[len] == ' ')
    {
        len--;
    }
    strncpy(trimmed, str, len + 1);
    trimmed[len + 1] = '\0';
}

char to_upper(char c)
{
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
}

bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

bool is_letter(char c)
{
    return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}

bool is_space(char c)
{
    return (c == ' ');
}

bool in_range(char c, char min, char max)
{
    return (c >= min) && (c <= max);
}

bool starts_with(const char* string, const char* prefix)
{
    return 0 == memcmp(string, prefix, strlen(prefix));
}

bool ends_with(const char* string, const char* suffix)
{
    int pos = strlen(string) - strlen(suffix);
    if (pos >= 0)
    {
        return 0 == memcmp(string + pos, suffix, strlen(suffix));
    }
    return false;
}

bool equals(const char* string1, const char* string2)
{
    return 0 == strcmp(string1, string2);
}

// Text message formatting:
//   - replaces lowercase letters with uppercase
//   - merges consecutive spaces into single space
void fmtmsg(char* msg_out, const char* msg_in)
{
    char c;
    char last_out = 0;
    while ((c = *msg_in))
    {
        if (c != ' ' || last_out != ' ')
        {
            last_out = to_upper(c);
            *msg_out = last_out;
            ++msg_out;
        }
        ++msg_in;
    }
    *msg_out = 0; // Add zero termination
}

char* append_string(char* string, const char* token)
{
    while (*token != '\0')
    {
        *string = *token;
        string++;
        token++;
    }
    *string = '\0';
    return string;
}

const char* copy_token(char* token, int length, const char* string)
{
    // Copy characters until a whitespace character or the end of string
    while (*string != ' ' && *string != '\0')
    {
        if (length > 0)
        {
            *token = *string;
            token++;
            length--;
        }
        string++;
    }
    // Fill up the rest of token with \0 terminators
    while (length > 0)
    {
        *token = '\0';
        token++;
        length--;
    }
    // Skip whitespace characters
    while (*string == ' ')
    {
        string++;
    }
    return string;
}

// Parse a 2 digit integer from string
int dd_to_int(const char* str, int length)
{
    int result = 0;
    bool negative;
    int i;
    if (str[0] == '-')
    {
        negative = true;
        i = 1; // Consume the - sign
    }
    else
    {
        negative = false;
        i = (str[0] == '+') ? 1 : 0; // Consume a + sign if found
    }

    while (i < length)
    {
        if (str[i] == 0)
            break;
        if (!is_digit(str[i]))
            break;
        result *= 10;
        result += (str[i] - '0');
        ++i;
    }

    return negative ? -result : result;
}

// Convert a 2 digit integer to string
void int_to_dd(char* str, int value, int width, bool full_sign)
{
    if (value < 0)
    {
        *str = '-';
        ++str;
        value = -value;
    }
    else if (full_sign)
    {
        *str = '+';
        ++str;
    }

    int divisor = 1;
    for (int i = 0; i < width - 1; ++i)
    {
        divisor *= 10;
    }

    while (divisor >= 1)
    {
        int digit = value / divisor;

        *str = '0' + digit;
        ++str;

        value -= digit * divisor;
        divisor /= 10;
    }
    *str = 0; // Add zero terminator
}

bool chkcall(const char* call)
{
    // TODO: trim a str


    size_t len = strlen(call);
    char base_call[len + 1];

    if (len > 11) return false;
    if (strchr(call, '.')) return false;
    if (strchr(call, '+')) return false;
    if (strchr(call, '-')) return false;
    if (strchr(call, '?')) return false;

    char *slash = strchr(call, '/');

    // if ((len > 6) && !slash) return false;

    if (!slash) {
        strcpy(base_call, call);
    } else {
        uint8_t pre_slash_len = slash - call;
        uint8_t post_slash_len = len - pre_slash_len - 1;
        if (pre_slash_len > post_slash_len) {
            strncpy(base_call, call, pre_slash_len);
            base_call[pre_slash_len] = '\0';
        } else {
            strncpy(base_call, slash + 1, post_slash_len);
            base_call[post_slash_len] = '\0';
        }
    }

    // Base call must be < 7 characters
    size_t base_call_len = strlen(base_call);

    if (starts_with(base_call, "3DA0") && (base_call_len > 4) && (base_call_len <= 7))
    {
        // Work-around for Swaziland prefix: 3DA0XYZ -> 3D0XYZ
        memcpy(base_call, "3D0", 3);
        memmove(base_call + 3, base_call + 4, base_call_len - 4);
        base_call_len--;
    }
    else if (starts_with(base_call, "3X") && is_letter(base_call[2]) && base_call_len <= 7)
    {
        // Work-around for Guinea prefixes: 3XA0XYZ -> QA0XYZ
        memcpy(base_call, "Q", 1);
        memmove(base_call + 1, base_call + 2, base_call_len - 2);
        base_call_len--;
    }

    if (base_call_len > 6) return false;

    // One of first two characters (c1 or c2) must be a letter
    if (!(is_letter(base_call[0]) || is_letter(base_call[1]))) return false;

    // Must have a digit in 2nd or 3rd position
    int8_t i_pos = -1;
    if (is_digit(base_call[1])) i_pos = 1;
    if (is_digit(base_call[2])) i_pos = 2;

    if (i_pos == -1) return false;

    // Callsign must have a suffix of 1-3 letters
    uint8_t n = 0;
    for (int8_t i = i_pos + 1; i < base_call_len; i++)
    {
        if (!is_letter(base_call[i])) return false;
        n++;
    }
    if ((n < 1) || (n > 3)) {
        return false;
    }
    return true;
}

char charn(int c, ft8_char_table_e table)
{
    if ((table != FT8_CHAR_TABLE_ALPHANUM) && (table != FT8_CHAR_TABLE_NUMERIC))
    {
        if (c == 0)
            return ' ';
        c -= 1;
    }
    if (table != FT8_CHAR_TABLE_LETTERS_SPACE)
    {
        if (c < 10)
            return '0' + c;
        c -= 10;
    }
    if (table != FT8_CHAR_TABLE_NUMERIC)
    {
        if (c < 26)
            return 'A' + c;
        c -= 26;
    }

    if (table == FT8_CHAR_TABLE_FULL)
    {
        if (c < 5)
            return "+-./?"[c];
    }
    else if (table == FT8_CHAR_TABLE_ALPHANUM_SPACE_SLASH)
    {
        if (c == 0)
            return '/';
    }

    return '_'; // unknown character, should never get here
}

// Convert character to its index (charn in reverse) according to a table
int nchar(char c, ft8_char_table_e table)
{
    int n = 0;
    if ((table != FT8_CHAR_TABLE_ALPHANUM) && (table != FT8_CHAR_TABLE_NUMERIC))
    {
        if (c == ' ')
            return n + 0;
        n += 1;
    }
    if (table != FT8_CHAR_TABLE_LETTERS_SPACE)
    {
        if (c >= '0' && c <= '9')
            return n + (c - '0');
        n += 10;
    }
    if (table != FT8_CHAR_TABLE_NUMERIC)
    {
        if (c >= 'A' && c <= 'Z')
            return n + (c - 'A');
        n += 26;
    }

    if (table == FT8_CHAR_TABLE_FULL)
    {
        if (c == '+')
            return n + 0;
        if (c == '-')
            return n + 1;
        if (c == '.')
            return n + 2;
        if (c == '/')
            return n + 3;
        if (c == '?')
            return n + 4;
    }
    else if (table == FT8_CHAR_TABLE_ALPHANUM_SPACE_SLASH)
    {
        if (c == '/')
            return n + 0;
    }

    // Character not found
    return -1;
}
