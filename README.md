<img src="Mhuixs/mhuclt/logo/Mhuixs-logo.png" height="150px" />    

# 正在开发的迷你宝藏数据库软件 QwQ

## 1. 介绍 : )

Mhuixs是一个超超超迷你的基于内存的数据库，别看它小小的，却有着大大的梦想！

它努力结合关系型与非关系型数据库的优点，立志提供国产化、可集成化、较为丰富的数据结构支持，还有超厉害的简单语言特性，是不是超酷的！


## 2. 背景 >,<

基于内存的高速数据库可是未来的大明星哟。Mhuixs 虽然只是个玩具级软件，但它的优势也不少呢：

- 既支持关系型的表数据管理，又支持非关系型的键值对池数据管理，简直是个 “全能小选手”。
- 它太小了！可集成化超厉害，未来想轻松内嵌到其他程序、甚至是操作系统内核里面呢！
- 立志于拥有较为丰富的数据结构支持，就像一个装满各种宝贝的数据结构百宝箱。
- 简单却安全的权限分级管理，数据安全就像被超级小卫士守护着！
- 追求简单却十分基础的语法，勇敢地抛弃复杂，你把语法文件发给AI看，AI直接现场给你学会



## 3. 当前总览 O.o

项目文件目录结构就像一个小小的玩具屋，每个房间都有自己的用途：

- `datstrc`：数据结构文件夹，包含表、队列等数据组织方式的相关文件。
- `mlib`：存放最基础的库文件，如Mhudef.h（基本参数定义文件）。
- `mhuclt`:客户端实现库函数，客户端命令行工具

模块框架介绍

| 序号 | 库名   | 模块功能             |
|------|--------|----------------------|
| 1    | tblh   | 表处理库             |
| 2    | kvalh  | 多维键值对处理库      |
| 3    | memh   | 内存分配库           |
| 4    | strmap | 字符串型局部空间分配函数库 |
| 5    | list   | 列表实现库           |
| 6    |session | 网络会话管理库    |
| 7    | zslih  | 压缩库           |
| 8   | Mhudef   | 基础定义库           |
| 9   | mhuix  | 数据库（基础设置）核心库 |
| 10   | thrdh| 多线程相关库         |
| 11   | bitmap  | 位图操作库           |
| 12   | hidat  | 加密库           |
| 13  | mhlang  | 语法定义库           |
| ...  | ...  | ...   |


## 4.合作 ^^)

如果你也觉得这个迷你数据库很可爱，想和我们一起玩，欢迎来找我们：

emial：hj18914255909@outlook.com

微信：wx17601516389


## 5.开始日期  : ; :

- 开始日期：2024.10.17，全国扶贫日，这是Mhuixs开始诞生的日子，就像小树苗种下的那天。
- readme更新时间：2025.2.14，这是小树苗又长大了一点，记录成长的新时刻。


## 6.NAQL草案 >~<

NAQL：NAture-language Query Language

旨在设计一种最接近口语的、最简单、给AI可以直接现场学会的数据查询语言。 

未来我们会出一版专门提供给AI看的查询语言 “ 学习资料 ”。



#### HOOK相关语法

关键字【HOOK】【TABLE,KVALOT,LIST,BITMAP,STREAM】【DEL,TYPE,RANK,CLEAR,DESC,GET,WHERE,TEMP】【$:预处理联系符】

{

    #HOOK基础操作
    
    [GET;] #标准 GET_OBJ 语句,获取当前操作对象的所有数据

    [WHERE;] #标准 WHERE 语句，返回当前操作对象的基本信息，返回一个json格式的字符串

    [DESC;] #标准 DESC 语句，返回当前操作对象的视图信息

    [HOOK;] #标准 HOOK 语句，回归HOOK根，此时无数据操作对象

    [HOOK objtype objname1 objname2 ...;] #标准 HOOK_MAKE 语句，使用钩子创建一个操作对象

    [HOOK objname;] #标准 HOOK_CHECKOUT 语句，手动切换到一个已经存在的操作对象

    [HOOK DEL objname1 objname2 ...;] #标准 HOOK_DEL_OGJ 语句，删除指定的HOOK操作对象,回归HOOK根，此时无数据操作对象

        衍生&&:HOOK DEL $WHERE;#删除当前操作对象            

    [HOOK CLEAR objname1 objname2 ...;]#标准 HOOK_CLEAR 语句，清空当前操作对象的所有数据,但保留操作对象及其信息

    [RANK objname rank;]#标准 RANK_OBJ 语句，设置指定HOOK操作对象的权限等级为rank

        衍生&&:RANK $WHERE rank;#设置当前对象的权限等级为rank
    
    [GET RANK objname;] #标准 GET_RANK 语句，获取指定HOOK操作对象的权限等级

        衍生&&:GET RANK $WHERE; #获取当前操作对象的权限等级

    [GET TYPE objname;] #标准 GET_TYPE 语句，获取指定HOOK操作对象的类型

        衍生&&:GET TYPE $WHERE; #获取当前操作对象的类型

    [TEMP GET id;]#标准 TEMP_GET 语句,创建临时数组,将这个临时数组取名为id,id<0 - 65535>

    [TEMP DEL id;]#标准 TEMP_DEL 语句,删除临时数组

    [TEMP WHERE id1 ==/-- id2 id3;] #标准 TEMP_WHERE 语句,取 id1 和 id2 的交集/并集放入临时数组 id3 中

    [WHERE field_index >/</==/>=/!= value id;] #标准 WHERE_CONDITION_TEMP 语句 查询指定字段的行数据,放入临时数组 id 中,然后再除去重复项,相当于取并集

    [GET TEMP id;]#标准 GET_TEMP 语句,获取临时数组id对应的行索引对应的行数据
}

#### TABLE类语法

关键字【INSERT,GET,FIELD,SET,ADD,SWAP,DEL,RENAME,ATTRIBUTE,POS,WHERE】
【[i1,int8_t],[i2,int16_t],[i4,int32_t,int],[i8,int64_t],[ui1,uint8_t],[ui2,uint16_t],[ui4,uint32_t],[ui8,uint64_t],
[f4,float],[f8,double],[str,stream],date,time,datetime】
【PKEY,FKEY,UNIQUE,NOTNULL,DEFAULT】
【$:将字段名预处理为索引】$field_name <==> field_index(数字索引从0开始)

{

    #FILED操作,不指明FIELD默认就是LINE操作

    [FIELD ADD field1_name datatype restraint field2_name datatype restraint...;]#标准 FIELD_ADD 语句

        衍生&&:FIELD ADD (field1_name datatype restraint,...);#从左向右添加字段,初处理将会对命令进行预处理，即‘,’替换为空格,()替换为空格

              预处理=> FIELD ADD  field1_name datatype restraint field2_name datatype restraint...;

    [FIELD INSERT field_index field_name datatype restraint;]#标准 FIELD_INSERT 语句只支持单次插入一个字段

        衍生&&:FIELD INSERT (field_name datatype restraint,...) AT field_index; #在指定位置插入多个字段

              预处理=> FIELD INSERT field_index field1_name datatype restraint;...#多个标准语句的组合

    [FIELD SWAP field1_index field2_index field3_index ...;]#标准 FIELD_SWAP 语句,1先后分别与2,3,...交换  

    [FIELD DEL field1_index field2_index ...;]#标准 FIELD_DEL 语句,删除指定字段

    [FIELD RENAME field_index field_name;]#标准 FIELD_RENAME 语句,重命名字段,只支持一次操作一个字段

        衍生&&:FIELD RENAME field_index field_name ...; #重命名字段 

              预处理=> FIELD RENAME field_index field_name;...#多个标准语句的组合

    [FIELD SET field_index ATTRIBUTE attribute;]#标准 FIELD_SET 语句,设置字段约束性属性
        
    [ADD value1 value2...;]#标准 LINE_ADD 语句

        衍生&&:ADD (line1_data,NULL,...)(line2_data,3,...)...; #在表末尾添加几行数据，数据按字段顺序依次给出，必须包含所有字段，NULL表示字段为NULL
              预处理=> ADD  line1_data NULL ...;ADD line2_data 3 ...;... #分解为多个ADD语句

        衍生&&:ADD field_index=value1 field_index=value2 ...;#表末添加一行数据，部分数据直接给出，其它部分自动补全为NULL

              预处理=> ADD  NULL NULL value1 value2 ...;#转变为ADD普通语句

    [INSERT line_index value1 value2 ...]#标准 LINE_INSERT 语句

        衍生&&:INSERT (line1_data,NULL,...) AT line_index; #在指定行号处插入一行数据，line_number从0开始计数

              预处理=> INSERT line1_index line1_data NULL...;INSERT line2_index line2_data 3...;... #分解为多个INSERT语句

        衍生&&:INSERT field_index=value1 field_index=value2... AT line_index;#表末添加一行数据，部分数据直接给出，其它部分自动补全为NULL

              预处理=> INSERT line_index  NULL NULL value1 value2...;#转变为ADD普通语句

    [SET line1_index field1_index value1 line2_index field2_index value2 ...;]#标准 LINE_SET 语句

        衍生&&:SET line_number field1_index = value1, field2_index = value2,...;#(,)等在第一步预处理时就会被替换为‘ ’

              预处理=> SET line_index field1_index value1 line_index field2_index value2...;#转变为SET普通语句

        衍生&&:SET POS (x1,y1)= value1 (x2,y2)=value2 ...;#(,)等在第一步预处理时就会被替换为‘ ’

              预处理=> SET line_index field1_index value1 line_index field2_index value2...;#转变为SET普通语句

    [DEL line1_index line2_index...;]#标准 LINE_DEL 语句

    [SWAP line1_index line2_index ...;]#标准 LINE_SWAP 语句

    [GET line1_index line2_index ...;]#标准 LINE_GET 语句,获取指定行号的多行数据

        GET FIELD field_name;#获取指定字段对应的列的数据

    [GET POS x1 y1 x2 y2 ...;]#标准 POS_GET 语句,获取指定坐标的数据

}

#### KVALOT类语法

关键字【SET,DEL,EXISTS,SELECT,KEY,TYPE】

{

    [EXISTS key1 key2 ...;] #标准 KEY_EXISTS 语句,判断存在几个键

    [SELECT pattern;] #标准 KEY_SELECT 语句，查找所有符合给定模式的键,注意查询的是键，不是值
        衍生&&:SELECT ALL;#查找所有键
              预处理=> SELECT *; #查找所有键

    [SET TYPE type key1 key2 key3 ... ;] #标准 KEY_SET 语句，不赋值，type为KVALOT,STREAM,TABLE,LIST,BITMAP

    [DEL key1 key2 ...;] #标准 KEY_DEL 语句,删除指定键
    
    [KEY key;]# 标准 KEY_CHECKOUT 语句,进入键对象操作
    
}

#### STREAM类语法
    
关键字【APPEND,SET,GET,LEN】

{

    [APPEND value;]#标准 STREAM_APPEND 语句，将数据追加向流中附加数据
    
    [APPEND pos value;]#标准 STREAM_APPEND_POS 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。

    [GET pos len;]#标准 STREAM_GET 语句，获取流中指定长度的数据

    [SET pos value;]#标准 STREAM_SET 语句，从pos处设置流中指定位置的值

    [SET pos len char;]#标准 STREAM_SET_CHAR 语句，从pos处设置流中指定位置的值,并将其长度设置为len

    [GET LEN;] #标准 STREAM_GET_LEN 语句，获取流的长度
}
    
#### LIST类语法
    
关键字【GET,DEL,LEN,INSERT,LPUSH,RPUSH,LPOP,RPOP,SET,EXISTS】

{

    [LPUSH value;]#标准 LIST_LPUSH 语句，在列表开头添加一个值

    [RPUSH value;]#标准 LIST_RPUSH 语句，在列表末尾添加一个值

    [LPOP;]#标准 LIST_LPOP 语句，移除并返回列表开头的值

    [RPOP;]#标准 LIST_RPOP 语句，移除并返回列表末尾的值


    [GET index;] #标准 LIST_GET 语句,获取指定位置的值,index:1,2..为第1,2..个元素，-1,-2,为倒数第1,2..个元素，0:所有元素
        衍生&&:GET ALL;#获取所有值

    [DEL index;] #标准 LIST_DEL 语句，删除指定索引位置的值

    [GET LEN index;]#标准 LIST_GET_INDEX_LEN 语句，获取列表中指定索引位置的值的长度

    [INSERT index value;] #标准 LIST_INSERT 语句，在指定索引位置插入一个值

    [SET index value;]#标准 LIST_SET 语句，更新列表中指定索引位置的值

    [EXISTS value1 value2 ...;]#标准 LIST_EXISTS 语句，判断列表是否存在指定值

    [GET LEN;] #标准 LIST_GET_LEN 语句，获取列表的长度

}

#### BITMAP类语法
    
关键字【SET,GET,COUNT】

{

    [SET offset value;] #标准 BITMAP_SET 语句，设置位图中指定偏移量处的位值，value为0或1

    [SET offset1 offset2 value;] #标准 BITMAP_SET_RANGE 语句，设置位图中指定偏移量范围内的位值，value为0或1

    [GET offset;] #标准 BITMAP_GET 语句，获取位图中指定偏移量处的位值

    [GET offset1 offset2;]#标准 BITMAP_GET_RANGE 语句，获取位图中指定偏移量范围内的位值

    [COUNT;] #标准 BITMAP_COUNT 语句，统计位图中值为1的位数

    [COUNT offset1 offset2;] #标准 BITMAP_COUNT_RANGE 语句，统计位图中指定偏移量范围内值为1的位数     
   
}
