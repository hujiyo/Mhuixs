#ifndef FCALL_HPP
#define FCALL_HPP

#include <map>

#include "Mhudef.hpp"
#include "stream.hpp"
#include "hook.hpp"
#include "merr.h"




int fun_call_init();
mrc Mhuixscall(UID caller,basic_handle_struct bhs,FunID funseq,int argc,void* argv[]);


#endif
