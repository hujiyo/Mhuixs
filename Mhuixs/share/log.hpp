#ifndef LOG_HPP
#define LOG_HPP

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "env.hpp"

#define open_log //是否开启日志

int logger_init();
void log(char* message);

#endif

