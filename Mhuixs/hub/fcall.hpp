#ifndef FCALL_HPP
#define FCALL_HPP

#include "Mhudef.hpp"
#include "stream.hpp"
#include "hook.hpp"
#include "merr.h"
#include "funseq.h"
#include "registry.hpp"
#include "usergroup.hpp"
#include "funseq.h"
#include "tblh.hpp"
#include "kvalh.hpp"
#include "list.hpp"
#include "bitmap.hpp"
#include "stream.hpp"
#include "netplug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void funcall_init();

#define FUNTABLE_SIZE 1024
typedef response_t* (*Mhuixsfun_t)(basic_handle_struct bhs,command_t* cmd);

/*
 只负责执行操作，其它不负责
 */
extern Mhuixsfun_t funtable[FUNTABLE_SIZE];


#endif
