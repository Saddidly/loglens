#include "loglens.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static int compare_patterns(const void *a, const void *b) {
    const ll_pattern *left = *(const ll_pattern *const *)a;
    const ll_pattern *right = *(const ll_pattern *const *)b;
    if (left->count != right->count) return left->count > right->count ? -1 : 1;
    if (left->level != right->level) return left->level < right->level ? -1 : 1;
    return strcmp(left->text, right->text);
}

static ll_pattern **sorted(const ll_analysis *analysis) {
    size_t i;
    ll_pattern **result;
    if (analysis->pattern_count == 0) return NULL;
    result = malloc(analysis->pattern_count * sizeof(*result));
    if (!result) return NULL;
    for (i = 0; i < analysis->pattern_count; ++i) result[i] = &analysis->patterns[i];
    qsort(result, analysis->pattern_count, sizeof(*result), compare_patterns);
    return result;
}

static void json_string(FILE *out, const char *text) {
    fputc('"', out);
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c == '"' || c == '\\') { fputc('\\', out); fputc(c, out); }
        else if (c >= 128) {
            size_t count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
            size_t i;
            int valid = count != 0;
            for (i = 0; valid && i < count; ++i) {
                unsigned char next = (unsigned char)text[i];
                if (next < 0x80 || next > 0xbf) valid = 0;
                if (i == 0 && ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) ||
                               (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90))) valid = 0;
            }
            if (valid) {
                fputc(c, out);
                for (i = 0; i < count; ++i) fputc((unsigned char)*text++, out);
            } else fprintf(out, "\\u%04x", c);
        }
        else if (c < 32 || c == 127) {
            fprintf(out, "\\u%04x", c);
        } else fputc(c, out);
    }
    fputc('"', out);
}

int ll_write_json(FILE *out, const ll_analysis *analysis, size_t top) {
    size_t i;
    ll_pattern **patterns = sorted(analysis);
    fprintf(out, "{\"schema_version\":1,\"lines\":%" PRIu64 ",\"matched\":%" PRIu64
            ",\"skipped_long\":%" PRIu64 ",\"skipped_binary\":%" PRIu64
            ",\"untracked_patterns\":%" PRIu64 ",\"levels\":{",
            analysis->lines, analysis->matched, analysis->skipped_long,
            analysis->skipped_binary, analysis->untracked_patterns);
    for (i = 0; i < LL_LEVEL_COUNT; ++i) {
        fprintf(out, "%s\"%s\":%" PRIu64, i ? "," : "", ll_level_name((ll_level)i), analysis->levels[i]);
    }
    fprintf(out, "},\"patterns\":[");
    for (i = 0; patterns && i < analysis->pattern_count && i < top; ++i) {
        ll_pattern *pattern = patterns[i];
        fprintf(out, "%s{\"count\":%" PRIu64 ",\"level\":\"%s\",\"truncated\":%s,\"template\":",
                i ? "," : "", pattern->count, ll_level_name(pattern->level), pattern->truncated ? "true" : "false");
        json_string(out, pattern->text);
        fputc('}', out);
    }
    fprintf(out, "],\"report_complete\":%s}\n", (analysis->pattern_count && !patterns) ||
            analysis->skipped_long || analysis->skipped_binary || analysis->untracked_patterns ? "false" : "true");
    {
        int complete = analysis->pattern_count == 0 || patterns != NULL;
        free(patterns);
        return complete && !ferror(out) ? 0 : -1;
    }
}

int ll_write_text(FILE *out, const ll_analysis *analysis, size_t top) {
    size_t i;
    ll_pattern **patterns = sorted(analysis);
    fprintf(out, "LogLens: %" PRIu64 " lines; %" PRIu64 " matched; %" PRIu64 " oversized; %" PRIu64 " binary\n",
            analysis->lines, analysis->matched, analysis->skipped_long, analysis->skipped_binary);
    for (i = 0; i < LL_LEVEL_COUNT; ++i)
        fprintf(out, "%-7s %" PRIu64 "\n", ll_level_name((ll_level)i), analysis->levels[i]);
    fprintf(out, "\nMost frequent normalized templates:\n");
    for (i = 0; patterns && i < analysis->pattern_count && i < top; ++i)
        fprintf(out, "%8" PRIu64 "  %-7s %s%s\n", patterns[i]->count,
                ll_level_name(patterns[i]->level), patterns[i]->text, patterns[i]->truncated ? " [prefix truncated]" : "");
    if (analysis->pattern_count && !patterns) fprintf(out, "Cannot allocate report ordering.\n");
    if (analysis->untracked_patterns)
        fprintf(out, "\n%" PRIu64 " lines had new templates beyond the configured capacity.\n", analysis->untracked_patterns);
    {
        int complete = analysis->pattern_count == 0 || patterns != NULL;
        free(patterns);
        return complete && !ferror(out) ? 0 : -1;
    }
}
