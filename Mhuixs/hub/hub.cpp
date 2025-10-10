#include "hub.hpp"
#include "netplug.h"
#include "usergroup.hpp"
#include "defpms.h"
#include "registry.hpp"
#include <stdatomic.h>

atomic_int hub_running = 0;

void freecmd(command_t* cmd){
    free(cmd->obj);
    free(cmd->k1);
    free(cmd->k2);
    free(cmd->k3);
    free(cmd);
}

int hub() {
    hub_running = 1;
    for(;;){
        command_t *cmd = NULL;
        response_t *resp = (response_t*)calloc(sizeof(response_t));
        if(command_queue.wait_dequeue(cmd);
        
        //关机命令
        if(cmd->command_id == CMD_SYSTEM_SHUTDOWN
            && cmd->session->user_id == 0){
            hub_running = 0;
            return 0;
        }
        //检查对象是否申请切换
        if(cmd->obj != NULL){
            //对象申请切换,先检查对象是否存在
            HOOK* hook = Reg.find_hook((string)cmd->obj);
            if(hook == NULL){
                //对象不存在
                freecmd(cmd);
                
                continue;
            }
            //权限检查
            if(Ugmanager.is_entitled(*hook,cmd->session->user_id,Mode_type::READ)){
                //权限检查通过
                cmd->session->current_hook = hook;
                //释放命令
                freecmd(cmd);
                continue;
            }
            else{
                //权限检查不通过
                freecmd(cmd);
                continue;
            }
            cmd->session->current_hook = (string)cmd->obj;
            //释放对象
            cmd->obj = NULL;
            //释放命令
            free(cmd);
        }
    }
    
    
    return 0;
}

