#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX

#include "nob.h"

Nob_Cmd cmd = { 0 };

#define BUILD_DIR "build/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    cmd_append(&cmd, "cc", "-DSLICE_TEST", "-Wall", "-Wextra", "-g", "-x", "c", "-o", BUILD_DIR "slice", "slice.h");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, BUILD_DIR "slice");
    if (!cmd_run(&cmd)) {
        return 1;
    }

    cmd_append(&cmd, "cc", "-DDA_TEST", "-Wall", "-Wextra", "-g", "-x", "c", "-o", BUILD_DIR "da", "da.h");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, BUILD_DIR "da");
    if (!cmd_run(&cmd)) {
        return 1;
    }

    cmd_append(&cmd, "cc", "-DLEXER_TEST", "-Wall", "-Wextra", "-g", "-x", "c", "-o", BUILD_DIR "lexer", "lexer.h");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, BUILD_DIR "lexer");
    if (!cmd_run(&cmd)) {
        return 1;
    }

    cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-g", "-c", "-o", BUILD_DIR "parser.o", "parser.c");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-g", "-c", "-o", BUILD_DIR "node.o", "node.c");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-g", "-c", "-o", BUILD_DIR "elrond.o", "elrond.c");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, "cc", "-o", BUILD_DIR "elrond",
        BUILD_DIR "elrond.o",
        BUILD_DIR "node.o",
        BUILD_DIR "parser.o");
    if (!cmd_run(&cmd)) {
        return 1;
    }
    cmd_append(&cmd, BUILD_DIR "elrond", "helloworld.elr");
    if (!cmd_run(&cmd)) {
        return 1;
    }

    return 0;
}
