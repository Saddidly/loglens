#ifndef LOGLENS_H
#define LOGLENS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define LL_PATTERN_LENGTH 512
typedef enum { LL_TRACE, LL_DEBUG, LL_INFO, LL_WARN, LL_ERROR, LL_FATAL, LL_UNKNOWN, LL_LEVEL_COUNT } ll_level;
typedef struct {
    size_t max_line;
    size_t max_patterns;
    int min_level; /* -1 includes unclassified lines. */
    const char *contains;
} ll_options;
typedef struct {
    char *text;
    uint64_t count;
    ll_level level;
    int truncated;
} ll_pattern;
typedef struct {
    ll_options options;
    uint64_t lines;
    uint64_t matched;
    uint64_t skipped_long;
    uint64_t skipped_binary;
    uint64_t untracked_patterns;
    uint64_t levels[LL_LEVEL_COUNT];
    ll_pattern *patterns;
    size_t pattern_count;
    size_t *slots;
    size_t slot_count;
} ll_analysis;

int ll_init(ll_analysis *analysis, const ll_options *options);
void ll_destroy(ll_analysis *analysis);
/* Returns -1 on allocation/I/O failure; already consumed input remains counted. */
int ll_consume(ll_analysis *analysis, FILE *input);
ll_level ll_detect_level(const char *line);
const char *ll_level_name(ll_level level);
int ll_parse_level(const char *value);
int ll_write_json(FILE *output, const ll_analysis *analysis, size_t top);
int ll_write_text(FILE *output, const ll_analysis *analysis, size_t top);
#endif
