#include "loglens.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int equal_word(const char *word, size_t length, const char *expected) {
    size_t i;
    if (strlen(expected) != length) return 0;
    for (i = 0; i < length; ++i)
        if (toupper((unsigned char)word[i]) != expected[i]) return 0;
    return 1;
}

const char *ll_level_name(ll_level level) {
    static const char *names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "UNKNOWN"};
    return (level >= 0 && level < LL_LEVEL_COUNT) ? names[level] : "UNKNOWN";
}

int ll_parse_level(const char *value) {
    int i;
    for (i = 0; i < LL_UNKNOWN; ++i)
        if (equal_word(value, strlen(value), ll_level_name((ll_level)i))) return i;
    if (equal_word(value, strlen(value), "WARNING")) return LL_WARN;
    if (equal_word(value, strlen(value), "CRITICAL")) return LL_FATAL;
    return -1;
}

ll_level ll_detect_level(const char *line) {
    const char *cursor = line;
    while (*cursor) {
        const char *start;
        size_t length;
        int i;
        if (!isalpha((unsigned char)*cursor) && *cursor != '_') { ++cursor; continue; }
        start = cursor;
        while (isalnum((unsigned char)*cursor) || *cursor == '_') ++cursor;
        length = (size_t)(cursor - start);
        for (i = 0; i < LL_UNKNOWN; ++i)
            if (equal_word(start, length, ll_level_name((ll_level)i))) return (ll_level)i;
        if (equal_word(start, length, "WARNING")) return LL_WARN;
        if (equal_word(start, length, "CRITICAL")) return LL_FATAL;
    }
    return LL_UNKNOWN;
}

int ll_init(ll_analysis *analysis, const ll_options *options) {
    memset(analysis, 0, sizeof(*analysis));
    if (!options || options->max_line == 0 || options->max_line > 16 * 1024 * 1024 ||
        options->max_patterns == 0 || options->max_patterns > 100000 ||
        options->min_level < -1 || options->min_level >= LL_UNKNOWN) return -1;
    analysis->options = *options;
    analysis->slot_count = options->max_patterns * 2 + 1;
    analysis->patterns = calloc(options->max_patterns, sizeof(*analysis->patterns));
    analysis->slots = calloc(analysis->slot_count, sizeof(*analysis->slots));
    if (!analysis->patterns || !analysis->slots) { ll_destroy(analysis); return -1; }
    return 0;
}

void ll_destroy(ll_analysis *analysis) {
    size_t i;
    for (i = 0; i < analysis->pattern_count; ++i) free(analysis->patterns[i].text);
    free(analysis->patterns);
    free(analysis->slots);
    memset(analysis, 0, sizeof(*analysis));
}

static int normalize(const char *line, char *output) {
    size_t length = 0;
    int pending_space = 0;
    while (*line) {
        unsigned char c = (unsigned char)*line;
        if (isspace(c)) { pending_space = length > 0; ++line; continue; }
        if (pending_space) {
            if (length + 1 >= LL_PATTERN_LENGTH) break;
            output[length++] = ' ';
            pending_space = 0;
        }
        if (length + 1 >= LL_PATTERN_LENGTH) break;
        if (isdigit(c)) {
            output[length++] = '#';
            do { ++line; } while (isdigit((unsigned char)*line));
        } else {
            output[length++] = (char)tolower(c);
            ++line;
        }
    }
    output[length] = '\0';
    return *line != '\0';
}

static uint64_t hash_pattern(const char *text, ll_level level, int truncated) {
    uint64_t hash = UINT64_C(14695981039346656037);
    while (*text) { hash ^= (unsigned char)*text++; hash *= UINT64_C(1099511628211); }
    hash ^= (unsigned)level + (unsigned)truncated * 16U;
    return hash;
}

static int consume_line(ll_analysis *analysis, const char *line) {
    ll_level level = ll_detect_level(line);
    char normalized[LL_PATTERN_LENGTH];
    int truncated;
    size_t slot;
    ll_pattern *pattern;
    ++analysis->levels[level];
    if (analysis->options.min_level >= 0 &&
        (level == LL_UNKNOWN || (int)level < analysis->options.min_level)) return 0;
    if (analysis->options.contains && !strstr(line, analysis->options.contains)) return 0;
    ++analysis->matched;
    truncated = normalize(line, normalized);
    slot = (size_t)(hash_pattern(normalized, level, truncated) % analysis->slot_count);
    while (analysis->slots[slot]) {
        pattern = &analysis->patterns[analysis->slots[slot] - 1];
        if (pattern->level == level && pattern->truncated == truncated && !strcmp(pattern->text, normalized)) {
            ++pattern->count;
            return 0;
        }
        slot = (slot + 1) % analysis->slot_count;
    }
    if (analysis->pattern_count == analysis->options.max_patterns) {
        ++analysis->untracked_patterns;
        return 0;
    }
    pattern = &analysis->patterns[analysis->pattern_count];
    pattern->text = malloc(strlen(normalized) + 1);
    if (!pattern->text) return -1;
    strcpy(pattern->text, normalized);
    pattern->count = 1;
    pattern->level = level;
    pattern->truncated = truncated;
    analysis->slots[slot] = ++analysis->pattern_count;
    return 0;
}

int ll_consume(ll_analysis *analysis, FILE *input) {
    char *line = malloc(analysis->options.max_line + 1);
    size_t length = 0;
    int overlong = 0, binary = 0, has_data = 0, c;
    if (!line) return -1;
    while ((c = fgetc(input)) != EOF) {
        has_data = 1;
        if (c == '\n') {
            ++analysis->lines;
            if (overlong) ++analysis->skipped_long;
            else if (binary) ++analysis->skipped_binary;
            else {
                line[length] = '\0';
                if (consume_line(analysis, line) < 0) { free(line); return -1; }
            }
            length = 0; overlong = 0; binary = 0; has_data = 0;
        } else {
            if (c == 0) binary = 1;
            if (length < analysis->options.max_line) line[length++] = (char)c;
            else overlong = 1;
        }
    }
    if (has_data) {
        ++analysis->lines;
        if (overlong) ++analysis->skipped_long;
        else if (binary) ++analysis->skipped_binary;
        else {
            line[length] = '\0';
            if (consume_line(analysis, line) < 0) { free(line); return -1; }
        }
    }
    free(line);
    return ferror(input) ? -1 : 0;
}
