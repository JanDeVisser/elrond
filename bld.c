
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX

#include "nob.h"

Nob_Cmd cmd = { 0 };

#define BUILD_DIR "build/"
#define SRC_DIR "src/"
#define RT_DIR "rt/arch/Darwin/arm64/"
#define TEST_DIR "test/"

#define STB_HEADERS(S)  \
    S(slice, SLICE)     \
    S(da, DA)           \
    S(io, IO)           \
    S(lexer, LEXER)     \
    S(cmdline, CMDLINE) \
    S(fs, FS)           \
    S(process, PROCESS) \
    S(resolve, RESOLVE) \
    S(json, JSON)

#define APP_HEADERS(S) \
    S(arm64)           \
    S(config)          \
    S(elrondlexer)     \
    S(ir)              \
    S(native)          \
    S(node)            \
    S(parser)          \
    S(operators)       \
    S(type)            \
    S(value)           \
    S(interpreter)

#define APP_SOURCES(S) \
    S(elrond)          \
    S(arm64)           \
    S(arm64_binop)     \
    S(generate)        \
    S(parser)          \
    S(operators)       \
    S(native)          \
    S(node)            \
    S(typespec)        \
    S(normalize)       \
    S(type)            \
    S(value)           \
    S(bind)            \
    S(stack)           \
    S(interpreter)     \
    S(execute)

#define RT_SOURCES(S) \
    S(endln)          \
    S(puthex)         \
    S(puti)           \
    S(putln)          \
    S(puts)           \
    S(strlen)         \
    S(to_string)

#define TEST_SOURCES(S) \
    S(01_helloworld)    \
    S(02_comptime)      \
    S(03_binexp)        \
    S(04_variable)      \
    S(05_add_variables) \
    S(06_assignment)    \
    S(07_while)

int format_sources()
{
    cmd_append(&cmd, "clang-format", "-i", "bld.c");
    if (!cmd_run(&cmd)) {
        return 1;
    }
#undef S
#define S(HDR, NAME)                                           \
    cmd_append(&cmd, "clang-format", "-i", SRC_DIR #HDR ".h"); \
    if (!cmd_run(&cmd)) {                                      \
        return 1;                                              \
    }
    STB_HEADERS(S)
#undef S
#define S(SRC)                                                 \
    cmd_append(&cmd, "clang-format", "-i", SRC_DIR #SRC ".c"); \
    if (!cmd_run(&cmd)) {                                      \
        return 1;                                              \
    }
    APP_SOURCES(S)
#undef S
#define S(HDR)                                                 \
    cmd_append(&cmd, "clang-format", "-i", SRC_DIR #HDR ".h"); \
    if (!cmd_run(&cmd)) {                                      \
        return 1;                                              \
    }
    APP_HEADERS(S)
#undef S
    return 0;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool        rebuild = false;
    char const *script = "helloworld.elr";
    bool        run = true;
    bool        format = false;

    for (int ix = 1; ix < argc; ++ix) {
        if (strcmp(argv[ix], "-B") == 0) {
            rebuild = true;
        }
        if ((strcmp(argv[ix], "-S") == 0) && (ix < argc - 1)) {
            script = argv[++ix];
        }
        if (strcmp(argv[ix], "--norun") == 0) {
            run = false;
        }
        if (strcmp(argv[ix], "format") == 0) {
            format = true;
        }
    }

    if (format) {
        return format_sources();
    }

    if (!nob_file_exists("build")) {
        mkdir_if_not_exists("build");
    }
    char *cc = getenv("CC");
    if (cc == NULL) {
        cc = "cc";
    }

    if (rebuild || nob_needs_rebuild1(BUILD_DIR "libelrstart.a", RT_DIR "start.s")) {
        cmd_append(&cmd, "as", RT_DIR "start.s", "-o", BUILD_DIR "start.o");
        if (!cmd_run(&cmd)) {
            return 1;
        }
        cmd_append(&cmd, "ar", "r", BUILD_DIR "libelrstart.a", BUILD_DIR "start.o");
        if (!cmd_run(&cmd)) {
            return 1;
        }
    }

    if (rebuild || nob_needs_rebuild1(BUILD_DIR "libtrampoline.a", RT_DIR "trampoline.s")) {
        cmd_append(&cmd, "as", RT_DIR "trampoline.s", "-o", BUILD_DIR "trampoline.o");
        if (!cmd_run(&cmd)) {
            return 1;
        }
        cmd_append(&cmd, "ar", "r", BUILD_DIR "libtrampoline.a", BUILD_DIR "trampoline.o");
        if (!cmd_run(&cmd)) {
            return 1;
        }
    }

    char const *rt_sources[] = {
        "", RT_DIR "syscalls.inc"
    };

    bool rt_sources_updated = rebuild;
#undef S
#define S(SRC)                                                                                                \
    rt_sources[0] = RT_DIR #SRC ".s";                                                                         \
    if (rebuild || nob_needs_rebuild(BUILD_DIR #SRC ".o", rt_sources, sizeof(rt_sources) / sizeof(char *))) { \
        cmd_append(&cmd, "as", "-o", BUILD_DIR #SRC ".o", RT_DIR #SRC ".s");                                  \
        if (!cmd_run(&cmd)) {                                                                                 \
            return 1;                                                                                         \
        }                                                                                                     \
        rt_sources_updated = true;                                                                            \
    }
    RT_SOURCES(S)
    if (rt_sources_updated) {
        cmd_append(&cmd, "ar", "r", BUILD_DIR "libelrrt.a",
#undef S
#define S(SRC) BUILD_DIR #SRC ".o",
            RT_SOURCES(S));
        if (!cmd_run(&cmd)) {
            return 1;
        }

        cmd_append(&cmd, "cc", "-dynamiclib", "-o", BUILD_DIR "libelrrt.dylib",
#undef S
#define S(SRC) BUILD_DIR #SRC ".o",
            RT_SOURCES(S));
        if (!cmd_run(&cmd)) {
            return 1;
        }
        cmd_append(&cmd, "install_name_tool", "-id", "@rpath/libelrrt.dylib", BUILD_DIR "libelrrt.dylib");
        if (!cmd_run(&cmd)) {
            return 1;
        }
    }

    bool headers_updated = rebuild;
#undef S
#define S(H, T)                                                                                                          \
    if (headers_updated || nob_needs_rebuild1(BUILD_DIR #H, SRC_DIR #H ".h")) {                                          \
        cmd_append(&cmd, cc, "-D" #T "_TEST", "-Wall", "-Wextra", "-g", "-x", "c", "-o", BUILD_DIR #H, SRC_DIR #H ".h"); \
        if (!cmd_run(&cmd)) {                                                                                            \
            return 1;                                                                                                    \
        }                                                                                                                \
        cmd_append(&cmd, BUILD_DIR #H);                                                                                  \
        if (!cmd_run(&cmd)) {                                                                                            \
            return 1;                                                                                                    \
        }                                                                                                                \
        headers_updated = true;                                                                                          \
    }
    STB_HEADERS(S)

    char const *sources[] = {
        "",
#undef S
#define S(H) SRC_DIR #H ".h",
        APP_HEADERS(S)
    };

    bool sources_updated = false;
#undef S
#define S(SRC)                                                                                                  \
    sources[0] = SRC_DIR #SRC ".c";                                                                             \
    if (headers_updated || nob_needs_rebuild(BUILD_DIR #SRC ".o", sources, sizeof(sources) / sizeof(char *))) { \
        cmd_append(&cmd, cc, "-Wall", "-Wextra", "-c", "-g", "-o", BUILD_DIR #SRC ".o", SRC_DIR #SRC ".c");     \
        if (!cmd_run(&cmd)) {                                                                                   \
            return 1;                                                                                           \
        }                                                                                                       \
        sources_updated = true;                                                                                 \
    }
    APP_SOURCES(S)
    if (sources_updated) {
        cmd_append(&cmd, cc, "-o", BUILD_DIR "elrond",
#undef S
#define S(SRC) BUILD_DIR #SRC ".o",
            APP_SOURCES(S) "-Lbuild", "-ltrampoline", "-lm");
        if (!cmd_run(&cmd)) {
            return 1;
        }
    }

    if (run) {
        nob_set_current_dir(TEST_DIR);
        // putenv("DYLD_LIBRARY_PATH=../" BUILD_DIR);
        int exit_code;
#undef S
#define S(T)                                               \
    cmd_append(&cmd, "../" BUILD_DIR "elrond", #T ".elr"); \
    if (!cmd_run(&cmd)) {                                  \
        return 1;                                          \
    }                                                      \
    cmd_append(&cmd, "./" #T);                             \
    exit_code = cmd_run(&cmd);
        TEST_SOURCES(S)
    }

    return 0;
}
