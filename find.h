#ifndef FIND_H
#define FIND_H

#include "cli.h"

void find_walk(const char *dir_path, const char *rel_path, const cli_opts_t *opts, int current_depth);

#endif /* FIND_H */
