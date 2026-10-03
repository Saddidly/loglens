#include "loglens.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

static FILE *open_input(const char *path) {
#ifdef _WIN32
    FILE *file;
    wchar_t *wide;
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (length <= 0) { errno = EINVAL; return NULL; }
    wide = malloc((size_t)length * sizeof(*wide));
    if (!wide) { errno = ENOMEM; return NULL; }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, length)) {
        free(wide); errno = EINVAL; return NULL;
    }
    file = _wfopen(wide, L"rb");
    free(wide);
    return file;
#else
    return fopen(path, "rb");
#endif
}

static void usage(void) {
    puts("Usage: loglens [OPTIONS] [FILE ...]\n"
         "With no files, reads stdin. Use - explicitly for stdin.\n"
         "  --json                 Machine-readable summary\n"
         "  --min-level LEVEL      TRACE DEBUG INFO WARN ERROR FATAL\n"
         "  --contains TEXT        Case-sensitive substring filter\n"
         "  --top N                Number of templates (default 20)\n"
         "  --max-patterns N       Capacity (default 5000; max 100000)\n"
         "  --max-line N           Bytes per line (default 1048576; max 16777216)\n"
         "  --fail-on-error        Exit 3 when any ERROR/FATAL line is present\n"
         "  --                     End option parsing");
}

static int parse_size(const char *value, size_t maximum, size_t *result) {
    char *end;
    uintmax_t number;
    const char *cursor;
    if (!*value || *value == '-') return -1;
    for (cursor = value; *cursor; ++cursor) if (*cursor < '0' || *cursor > '9') return -1;
    errno = 0;
    number = strtoumax(value, &end, 10);
    if (errno || *end || number == 0 || number > maximum) return -1;
    *result = (size_t)number;
    return 0;
}

static int run(int argc, const char *const *argv) {
    ll_options options = {1048576, 5000, -1, NULL};
    ll_analysis analysis;
    size_t top = 20;
    int json = 0, fail_error = 0, end_options = 0, file_count = 0, stdin_used = 0;
    int i, status = 0;
    const char **files = calloc((size_t)argc, sizeof(*files));
    if (!files) { fputs("loglens: cannot allocate arguments\n", stderr); return 2; }
    for (i = 1; i < argc; ++i) {
        const char *argument = argv[i];
        if (!end_options && !strcmp(argument, "--")) { end_options = 1; continue; }
        if (!end_options && (!strcmp(argument, "--help") || !strcmp(argument, "-h"))) { usage(); free(files); return 0; }
        if (!end_options && !strcmp(argument, "--version")) { puts("loglens 1.0.0"); free(files); return 0; }
        if (!end_options && !strcmp(argument, "--json")) json = 1;
        else if (!end_options && !strcmp(argument, "--fail-on-error")) fail_error = 1;
        else if (!end_options && (!strcmp(argument, "--contains") || !strcmp(argument, "--min-level") ||
                 !strcmp(argument, "--top") || !strcmp(argument, "--max-line") || !strcmp(argument, "--max-patterns"))) {
            if (++i == argc) { fprintf(stderr, "loglens: missing value for %s\n", argument); status = 2; break; }
            if (!strcmp(argument, "--contains")) options.contains = argv[i];
            else if (!strcmp(argument, "--min-level")) {
                options.min_level = ll_parse_level(argv[i]);
                if (options.min_level < 0) { fputs("loglens: invalid level\n", stderr); status = 2; break; }
            } else {
                size_t *target = !strcmp(argument, "--top") ? &top :
                    !strcmp(argument, "--max-line") ? &options.max_line : &options.max_patterns;
                const size_t maximum = !strcmp(argument, "--max-line") ? 16777216 : 100000;
                if (parse_size(argv[i], maximum, target) < 0) {
                    fprintf(stderr, "loglens: invalid value for %s\n", argument); status = 2; break;
                }
            }
        } else if (!end_options && argument[0] == '-' && strcmp(argument, "-")) {
            fprintf(stderr, "loglens: unknown option %s\n", argument); status = 2; break;
        } else files[file_count++] = argument;
    }
    if (status) { free(files); return status; }
    if (ll_init(&analysis, &options) < 0) { free(files); fputs("loglens: cannot initialize analysis\n", stderr); return 2; }
    if (!file_count) files[file_count++] = "-";
    for (i = 0; i < file_count; ++i) {
        FILE *input;
        if (!strcmp(files[i], "-")) {
            if (stdin_used) { fputs("loglens: stdin specified more than once\n", stderr); status = 1; continue; }
            stdin_used = 1; input = stdin;
#ifdef _WIN32
            if (_setmode(_fileno(stdin), _O_BINARY) < 0) { fputs("loglens: cannot use binary stdin\n", stderr); status = 1; continue; }
#endif
        } else input = open_input(files[i]);
        if (!input) { fprintf(stderr, "loglens: %s: %s\n", files[i], strerror(errno)); status = 1; continue; }
        if (ll_consume(&analysis, input) < 0) { fprintf(stderr, "loglens: cannot finish reading %s\n", files[i]); status = 1; }
        if (input != stdin && fclose(input) != 0) { fprintf(stderr, "loglens: cannot close %s\n", files[i]); status = 1; }
    }
    if (json) { if (ll_write_json(stdout, &analysis, top) < 0) status = 1; }
    else { if (ll_write_text(stdout, &analysis, top) < 0) status = 1; }
    if (fflush(stdout) != 0 || ferror(stdout)) status = 1;
    if (analysis.skipped_long || analysis.skipped_binary || analysis.untracked_patterns) status = 1;
    if (!status && fail_error && (analysis.levels[LL_ERROR] || analysis.levels[LL_FATAL])) status = 3;
    ll_destroy(&analysis);
    free(files);
    return status;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    int i, status = 2;
    char **arguments = calloc((size_t)argc, sizeof(*arguments));
    if (!arguments) { fputs("loglens: cannot allocate arguments\n", stderr); return 2; }
    for (i = 0; i < argc; ++i) {
        int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, NULL, 0, NULL, NULL);
        if (length <= 0) break;
        arguments[i] = malloc((size_t)length);
        if (!arguments[i] || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1,
                                                arguments[i], length, NULL, NULL)) break;
    }
    if (i == argc) status = run(argc, (const char *const *)arguments);
    else fputs("loglens: cannot decode arguments\n", stderr);
    for (i = 0; i < argc; ++i) free(arguments[i]);
    free(arguments);
    return status;
}
#else
int main(int argc, char **argv) { return run(argc, (const char *const *)argv); }
#endif
