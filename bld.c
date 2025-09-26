#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX

#include "nob.h"

Nob_Cmd cmd = { 0 };

#define BUILD_DIR "build/"

#define STB_HEADERS(S)  \
    S(slice, SLICE)     \
    S(da, DA)           \
    S(io, IO)           \
    S(lexer, LEXER)     \
    S(cmdline, CMDLINE) \
    S(fs, FS)           \
    S(process, PROCESS)

#define APP_HEADERS(S) \
    S(arm64)           \
    S(elrondlexer)     \
    S(ir)              \
    S(node)            \
    S(parser)          \
    S(operators)       \
    S(type)            \
    S(value)
#define NUM_HEADERS 8
#define NUM_SOURCES 9

#define APP_SOURCES(S) \
    S(elrond)          \
    S(arm64)           \
    S(generate)        \
    S(parser)          \
    S(operators)       \
    S(node)            \
    S(typespec)        \
    S(normalize)       \
    S(type)            \
    S(value)           \
    S(bind)

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_file_exists("build")) {
        mkdir_if_not_exists("build");
    }        
    char *cc = getenv("CC");
    if (cc == NULL) {
        cc = "cc";
    }
    bool headers_updated = false;
#undef S
#define S(H, T)                                                                                                  \
    if (headers_updated || nob_needs_rebuild1(BUILD_DIR #H, #H ".h")) {                                          \
        cmd_append(&cmd, cc, "-D" #T "_TEST", "-Wall", "-Wextra", "-g", "-x", "c", "-o", BUILD_DIR #H, #H ".h"); \
        if (!cmd_run(&cmd)) {                                                                                    \
            return 1;                                                                                            \
        }                                                                                                        \
        cmd_append(&cmd, BUILD_DIR #H);                                                                          \
        if (!cmd_run(&cmd)) {                                                                                    \
            return 1;                                                                                            \
        }                                                                                                        \
        headers_updated = true;                                                                                  \
    }
    STB_HEADERS(S)

    char const *sources[] = {
        "",
#undef S
#define S(H) #H ".h",
        APP_HEADERS(S)
    };

    bool sources_updated = false;
#undef S
#define S(SRC)                                                                                      \
    sources[0] = #SRC ".c";                                                                         \
    if (headers_updated || nob_needs_rebuild(BUILD_DIR #SRC ".o", sources, 9)) {                    \
        cmd_append(&cmd, cc, "-Wall", "-Wextra", "-g", "-c", "-o", BUILD_DIR #SRC ".o", #SRC ".c"); \
        if (!cmd_run(&cmd)) {                                                                       \
            return 1;                                                                               \
        }                                                                                           \
        sources_updated = true;                                                                     \
    }
    APP_SOURCES(S)
    if (sources_updated) {
        cmd_append(&cmd, cc, "-o", BUILD_DIR "elrond",
#undef S
#define S(SRC) BUILD_DIR #SRC ".o",
            APP_SOURCES(S) "-lm");
        if (!cmd_run(&cmd)) {
            return 1;
        }

        cmd_append(&cmd, BUILD_DIR "elrond", "helloworld.elr");
        if (!cmd_run(&cmd)) {
            return 1;
        }
    }

    return 0;
}
