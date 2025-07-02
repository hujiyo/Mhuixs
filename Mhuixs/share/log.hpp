#ifndef LOG_HPP
#define LOG_HPP

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "merr.h"
#include "env.hpp"

mrc logger_init();
void log(char* message);

#endif

