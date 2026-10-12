#include "scanmatch_ffi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    if (argc < 2) {
        fprintf(stderr, "Usage: scanmatch_cli <text_or_file> [catalog.csv]\n");
        return 2;
    }

    const char *input = argv[1];
    char *utf8_cmd_input = NULL;
#ifdef _WIN32
    int wargc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv && wargc >= 2) {
        int needed = WideCharToMultiByte(CP_UTF8, 0, wargv[1], -1, NULL, 0, NULL, NULL);
        if (needed > 0) {
            utf8_cmd_input = (char *)malloc(needed);
            if (utf8_cmd_input) {
                WideCharToMultiByte(CP_UTF8, 0, wargv[1], -1, utf8_cmd_input, needed, NULL, NULL);
                input = utf8_cmd_input;
            }
        }
        LocalFree(wargv);
    }
#endif

    const char *catalog = (argc >= 3) ? argv[2] : "data/products.csv";

    // If input is a file that exists, read its contents; otherwise treat as raw text
    char *text_content = NULL;
    FILE *f = fopen(input, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz >= 0 && sz < 10 * 1024 * 1024) {
            text_content = (char *)malloc(sz + 1);
            if (text_content) {
                size_t read_bytes = fread(text_content, 1, sz, f);
                text_content[read_bytes] = '\0';
            }
        }
        fclose(f);
    }

    const char *text_to_process = text_content ? text_content : input;

    SM_Context *c = sm_create_context();
    if (!c) {
        free(text_content);
        return 3;
    }

    if (sm_load_catalog_csv_ffi(c, catalog) != 0) {
        // Fallback for running from build directory
        if (sm_load_catalog_csv_ffi(c, "../data/products.csv") != 0) {
            fprintf(stderr, "Error: %s\n", sm_last_error(c));
            sm_destroy_context(c);
            free(text_content);
            return 4;
        }
    }

    char *json = NULL;
    int rc = sm_process_text_json(c, text_to_process, &json);
    if (rc != 0) {
        fprintf(stderr, "Error: %s\n", sm_last_error(c));
        sm_destroy_context(c);
        free(text_content);
        return 5;
    }

    if (json) {
        puts(json);
        sm_free_string(json);
    }

    sm_destroy_context(c);
    free(text_content);
    if (utf8_cmd_input) free(utf8_cmd_input);
    return 0;
}
