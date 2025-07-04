/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
为了避免由于错误操作引发的问题，
table需要增添表格恢复功能
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "env.hpp"//环境变量模块
#include "log.hpp"//日志模块
#include "getid.hpp"//ID分配器模块
#include "usergroup.hpp"//用户组管理模块

#include "Mhudef.hpp"
/*
下面的stack和queue是完全可以被list替换的，
我感觉stack和queue可以被优化掉
在最新的速度测试中，基于strmap.h进行内存分配的list库
的执行速度要比基于calloc和realloc的stack和queue快一点
*/


#include "mlib/session.hpp"

QUEUE run_queue;//执行队列execution queue
#define commend_size 32 //命令长度
#define run_queue_len 1000 //执行队列长度
/*
这个列表（队列）由“接入模块”负责维护
接入模块将特殊命令放入执行队列

命令格式：每个都是4字节
|     1    |    2   |   3   |   4  |   5   |   6   |   7  |   8   | 
| 命令类型  | 对象名 | 参数1 | 参数2 | 参数3 | 参数4 | 参数5 | 参数6 |
*/


QUEUE ret_queue;//发送返回队列return queue
#define retinf_size 1024  //一般返回的数据都应该比较大
#define ret_queue_len 100 //返回数据的队列长度
/*
这个列表（队列）由执行模块直接维护，
由发送模块负责取出数据进行整合发送给客户端,其中list适合存放字节流
*/

QUEUE log_queue;//日志队列log queue
/*
这个列表（队列）由执行模块直接维护。可以由
特殊命令和权限读取发送给客户端,list适合存放字节流
*/

TABLE hook_register;//钩子注册表hook register
/*
存储在Mhuixs数据库的所有数据结构都需要使用钩子进行引用
每重新定义一个数据结构，mhuixs的钩子注册表中就会自动添加一个钩子
作用：
1.很好的防止用户不小心忘记钩子名称，导致无法访问指定数据结构
  究竟在内存的哪个位置了。注意，这是非常危险的，因为相当于有
  一块数据占着内存却无法访问。
2.可以更加简单的对数据结构进行权限控制，防止用户恶意访问数据结构
  例如：用户无法访问管理员的钩子
3.数据压缩和储存在磁盘中的基本单位都是hook，比如，某个钩子长时间
  没有被使用，那么就可以将这个钩子对应的数据结构进行压缩，从而节省

hook_register的字段名(类型)暂定为：
hookname(s50) |  hooktype(ui1) |  hookrank(ui1) | hookcompressrank(ui1) | hookaddr(ui4) 
hookname:钩子名称
hooktype:钩子类型
hookrank:钩子等级
hookcompressrank:钩子压缩等级
hookaddr:钩子地址
*/


/*
这里是Mhuixs服务端的真正main函数
*/
int main()
{
    //环境变量模块
    if (env_init() != 0)
    {
        printf("\nENV module failed!\n");
        return 1;
    }

    //初始化日志模块
    if (logger_init() != 0)
    {
        printf("\nLogger module failed!\n");
        return 1;
    }

    //id分配器模块
    if (id_alloc_init() != 0)
    {
        printf("\nID allocator module failed!\n");
        return 1;
    }

    //用户组管理模块
    if (init_User_group_manager() != 0)
    {
        printf("\nUser group manager module failed!\n");
        return 1;
    }

    //初始化执行队列、发送队列、日志队列
    initLIST_AND_SET_BLOCK_SIZE_NUM(&run_queue,commend_size,run_queue_len);
    /*
    字节对齐问题：
    commend_size是4的倍数，所以使用strmap.h进行内存分配的内存是4的倍数
    */
    initLIST_AND_SET_BLOCK_SIZE_NUM(&ret_queue,retinf_size,run_queue_len);
    initLIST(&log_queue);
    //初始化钩子注册表
    {
        FIELD hook_register[5];
        initFIELD(&hook_register[0],"hookname",s50);
        initFIELD(&hook_register[1],"hooktype",ui1);
        initFIELD(&hook_register[2],"hookrank",ui1);
        initFIELD(&hook_register[3],"hookcompressrank",ui1);
        initFIELD(&hook_register[4],"hookaddr",ui4);
        tblh_make_table(&hook_register,"hook register",hook_register,5);
    }
    //伪代码
    /*
    启动监听线程，端口7777
    启动发送线程，端口9999    
    */
    //启动监听线程
    //主线程:
    uint32_t period=0;
    for(int i = 1000;i>0;i--){
        if(run_queue.num > 0){
            for(period = run_queue.num;period>0;period--){
                
                //getttaskfromrunqueue
                //执行任务
                //flashret_queue
                //flashlog_queue
            }
        }
        else{
            //如果run_queue为空，那么就等待10ms
            //这里可以使用sleep函数，也可以使用usleep函数
            //usleep(10000);
        }
    }
}
