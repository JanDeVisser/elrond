/*
 * Copyright (c) 2023, 2025 Jan de Visser <jan@finiandarcy.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __IO_H__
#define __IO_H__

#ifdef IO_TEST
#define IO_IMPLEMENTATION
#define SLICE_IMPLEMENTATION
#define DA_IMPLEMENTATION
#endif

#include "da.h"

extern opt_sb_t slurp_file(slice_t path);

#endif /* __IO_H__ */

#ifdef IO_IMPLEMENTATION
#ifndef IO_IMPLEMENTED

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

opt_sb_t slurp_file(slice_t path)
{
    sb_t copy = { 0 };
    if (!slice_is_cstr(path)) {
        sb_append(&copy, path);
        path = sb_as_slice(copy);
    }
    opt_sb_t ret = OPTNULL(sb_t);

    int fh = open(path.items, O_RDONLY);
    if (fh < 0) {
        goto done;
    }
    ssize_t sz = (ssize_t) lseek(fh, 0, SEEK_END);
    if (sz < 0) {
        goto done_close;
    }
    if (lseek(fh, 0, SEEK_SET) < 0) {
        goto done_close;
    }

    sb_t contents = { 0 };
    dynarr_ensure(&contents, sz + 1);
    contents.items[sz] = 0;
    if (read(fh, contents.items, sz) < sz) {
        sb_free(&contents);
        goto done_close;
    }
    contents.len = sz;
    ret = OPTVAL(sb_t, contents);

done_close:
    close(fh);

done:
    sb_free(&copy);
    return ret;
}

#endif /* IO_IMPLEMENTED */
#endif /* IO_IMPLEMENTATION */

#ifdef IO_TEST

int main()
{
    opt_sb_t contents = slurp_file(C("io.h"));
    assert(contents.ok);
    return 0;
}

#endif /* IO_TEST */
