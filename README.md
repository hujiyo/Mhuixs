# Mhuixs

## 项目介绍

Mhuixs是一个基于内存的数据库，旨在结合关系型与非关系型数据库的优势，提供国产化、可集成化、丰富的数据结构支持以及高级的安全特性。

## 项目背景

随着信息化战争的趋势，基于内存的高速数据库将成为未来主流。Mhuixs的优势将包括：

- 支持关系型与非关系型数据管理
- 可集成化，能够内嵌到其他程序、操作系统中。
- 丰富的数据结构支持
- 高安全特性，权限分级管理

## 项目结构

### 文件目录结构

项目文件目录结构如下：

- `datstrc`：数据结构文件夹，包含表、队列等数据组织方式的相关文件。
- `mlib`：存放最基础的库文件，如Mhudef.h（基本参数定义文件）。
- `mhuclt`:客户端实现库函数，客户端命令行工具



## 开始日期与更新时间

- 开始日期：2024.10.17
- readme更新时间：2025.1.18

## 模块框架介绍

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

## 合作：

emial：hj18914255909@outlook.com

微信：wx17601516389

## NAQL语法草案:NAture-language Query Language

旨在设计一种最接近口语的、最简单的数据查询语言。 

## 下面是HOOK所有使用方法

### 关键字【HOOK】【TABLE,KVALOT,LIST,BITMAP,STREAM】【DEL,TYPE,RANK,CLEAR,DESC,GET,WHERE,TEMP】【$:预处理联系符】

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

## 下面是操作对象为TABLE时的所有语法

### 关键字【INSERT,GET,FIELD,SET,ADD,SWAP,DEL,RENAME,ATTRIBUTE,POS,WHERE】
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

## KVALOT对象操作语法

### 关键字【KVALOT】【HOOK】【SET,GET,DEL,INCR,DECR,EXISTS,SELECT,ALL,APPEND,FROM,KEY,TYPE,LEN】

【KVALOT,STREAM,TABLE,LIST,BITMAP】

{
    
    #基础操作
    
    HOOK KVALOT mykvalot; #创建一个名为mykvalot的键值对存储对象，此时操作对象为mykvalot
    
    EXISTS key1 key2 ...; #判断存在几个键
    
    SELECT pattern; #查找所有符合给定模式的键,注意查询的是键，不是值
    
    SELECT ALL;#查找所有键
    
    SET key1 value1 key2 value2 ...; #设置stream类型的键值对，若键已存在则覆盖
    
    SET key1 key2 key3 ... TYPE type; #设置键值对的类型，type为KVALOT,STREAM,TABLE,LIST,BITMAP
    
    APPEND key value;# 将value追加到key的值中。
    
    APPEND key value pos;# 将value追加到key的值中，从指定位置开始。若pos超出key值的长度，则从key值的末尾开始追加。
    
    GET key1 key2 ...; #获取指定键的值
    
    GET key0 FROM start end;#获取指定键的值的子字符串 
    
    DEL key1 key2 ...; #删除指定键
    
    INCR KEY num; #对指定键的值进行递增操作，num为可选参数，默认递增1
    
    DECR KEY num; #对指定键的值进行递减操作，num为可选参数，默认递减1
    
    TYPE key1 key2 ...; #获取指定键的值的数据类型
    
    LEN key1 key2 ...; #获取指定键的值的长度
    
    #转入键对象操作
    
    KEY key0;# 进入键对象操作

}

## STREAM对象操作语法
    
### 关键字【STREAM】【HOOK】【ADD,GET,RANGE,DEL】

{
    
    HOOK STREAM mystream; #创建一个名为mystream的流对象，此时操作对象为mystream
    
    APPEND value; #向流中附加数据
    
    APPEND value pos;# 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。
    
    GET FROM start end;#获取流中指定范围的数据
    
    GET pos len;#获取流中指定长度的数据
    
    SET pos value;# 从pos处设置流中指定位置的值
    
    SET char FROM start TO end;# 从start到end处设置流中指定范围的值

}
    
## LIST对象操作语法
    
### 关键字【LIST】【HOOK】【ADD,GET,DEL,LEN,INSERT,UPDATE,LPUSH,RPUSH,LPOP,RPOP,FROM,ALL,TO,WHOIS,AT,SET,EXISTS】

{
    
    HOOK LIST mylist; #创建一个名为mylist的列表对象，此时操作对象为mylist
    
    LPUSH value1 value2 ...;#在列表开头添加一个值
    
    RPUSH/ADD value value2 ...;#在列表末尾添加一个值
    
    LPOP/GET; #移除并返回列表开头的值
    
    RPOP; #移除并返回列表末尾的值


    GET index1 index2 ...; #获取指定位置的值,index:1,2..为第1,2..个元素，-1,-2,为倒数第1,2..个元素
    
    GET ALL;/GET 0;#获取列表的所有元素
    
    GET FROM index1 TO index2; #获取指定范围内的值
    
    DEL index1 index2 ...; #删除指定索引位置的值
    
    DEL FROM index1 TO index2; #删除指定范围内的值
    
    DEL ALL;#删除所有值
    
    DEL WHOIS value; #删除含有指定值的所有元素
    
    LEN; #获取列表的元素个数
    
    LEN index;#获取列表中指定索引位置的值
    
    INSERT value AT index;/INSERT index value; #在指定索引位置插入一个值
    
    SET value AT index;/SET index value; #更新列表中指定索引位置的值
    
    EXISTS value1 value2 ...; #判断集合中是否存在指定值

}


## BITMAP对象操作语法
    
### 关键字【BITMAP】【HOOK】【SETBIT,GETBIT,COUNT,BITOP】

{
    
    HOOK BITMAP mybitmap; #创建一个名为mybitmap的位图对象，此时操作对象为mybitmap
    
    HOOK mybitmap;# 手动切换到一个已经存在的操作对象
    
    KEY BITMAP key; #创建一个名为key的键对象，此时操作对象为key
    
    KEY key;# 手动切换到一个已经存在的操作对象



    SET offset value;/SET value AT index; #设置位图中指定偏移量处的位值，value为0或1
    
    SET FROM offset TO offset value;/SET value FROM offset TO offset; #设置位图中指定偏移量范围内的位值，value为0或1
    
    GET offset; #获取位图中指定偏移量处的位值
    
    GET FROM offset TO offset;#获取位图中指定偏移量范围内的位值
    
    COUNT; #统计位图中值为1的位数
    
    COUNT offset1 offset2; #统计位图中指定偏移量范围内值为1的位数        

}
