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

#include "mlib/session.hpp"

//执行队列execution queue
//命令长度
//执行队列长度
/*
这个列表（队列）由“接入模块”负责维护
接入模块将特殊命令放入执行队列

命令格式：每个都是4字节
|     1    |    2   |   3   |   4  |
| 命令编号  | 对象名 | 参数数目 | void** |
当不指向任何对象时，对象默认为NULL
*/


//发送返回队列return queue
//一般返回的数据都应该比较大
//返回数据的队列长度
/*
这个列表（队列）由执行模块直接维护，
由发送模块负责取出数据进行整合发送给客户端,其中list适合存放字节流
*/

/*
这个列表（队列）由执行模块直接维护。可以由
特殊命令和权限读取发送给客户端,list适合存放字节流
*/

/*
存储在Mhuixs数据库的所有数据结构都需要使用钩子进行引用
每重新定义一个数据结构，mhuixs的钩子注册表中就会自动添加一个钩子
作用：
1.很好的防止用户不小心忘记钩子名称，导致无法访问指定数据结构
  究竟在内存的哪个位置了。注意，这是非常危险的，因为相当于有
  一块数据占着内存却无法访问。
2.HOOK可以更加简单的对数据结构进行权限控制，防止用户恶意修改数据结构
3.数据压缩和储存在磁盘中的基本单位都是hook，比如，某个钩子长时间
  没有被使用，那么就可以将这个钩子对应的数据结构进行压缩，从而节省内存
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
    
    //伪代码
    /*
    启动session模块 
    */
    //启动监听线程
    //主线程:
    //不断的在轮询每个会话，如果会话有任务，则执行任务
}
