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

##### 下面是HOOK所有使用方法@NAQL

    关键字【HOOK,GET,WHERE】【TABLE,KVALOT,LIST,BITMAP,STREAM】【DEL,TYPE,RANK,CLEAR】

    {

        #HOOK基础操作

        WHERE; #返回当前操作对象的信息，返回一个json格式的字符串
        
        HOOK; #回归HOOK根，此时无数据操作对象
        
        HOOK objtype objname1 objname2 ...; #使用钩子创建一个操作对象
        
        HOOK objname; #手动切换到一个已经存在的操作对象
        
        HOOK DEL WHERE; #删除本操作对象,退回HOOK根
        
        HOOK CLEAR objname1 objname2 ...;#清空当前操作对象的所有数据,但保留操作对象及其信息
        
        HOOK DEL objname1 objname2 ...; #删除指定的HOOK操作对象
        
        RANK WHERE rank; #设置当前对象的权限等级为rank
        
        RANK objname rank; #设置指定HOOK操作对象的权限等级为rank
        
        GET TYPE WHERE; #获取当前操作对象的类型
        
        GET RANK; #获取当前操作对象的权限等级
        
        GET RANK objname; #获取指定HOOK操作对象的权限等级
        
        GET TYPE objname; #获取指定HOOK操作对象的类型
        
        #HOOK高阶操作
        
        HOOK objname1 objname2; #启动多对象操作

    }

##### 下面是操作对象为TABLE时的所有语法@MHU
    
    关键字【TABLE】【HOOK】【INSERT,SELECT,UPDATE,GET,FIELD,SET,ADD,SWAP,DEL,RENAME,ATTRIBUTE,COORDINATE,AT,DESC,ALL】
    【[i1,int8_t],[i2,int16_t],[i4,int32_t,int],[i8,int64_t],[ui1,uint8_t],[ui2,uint16_t],[ui4,uint32_t],[ui8,uint64_t],
    [f4,float],[f8,double],[str,stream],date,time,datetime】
    【PKEY,FKEY,UNIQUE,NOTNULL,DEFAULT】
    
    {
        
        HOOK TABLE mytable; #创建一个名为mytable的表,此时操作对象为mytable
        
        #FILED操作
        
        FIELD ADD (field1_name datatype restraint,...);#从左向右添加字段
        
        FIELD INSERT (field1_name datatype restraint) AT field_number; #在指定位置插入字段
        
        FIELD SWAP field1_name field2_name; #交换两个字段
        
        FIELD DEL field1_name field2_name ...; #删除字段
        
        FIELD RENAME field1_name field2_name ...; #重命名字段
        
        FIELD SET field1_name ATTRIBUTE attribute; #重新设置字段约束性属性
        
        #LINE操作,不指明FIELD,默认就是整个LINE进行操作
        
        ADD (line1_data,NULL,...); #在表末尾添加一行数据，数据按照字段顺序依次给出，必须包含所有字段，没有的数据则采用NULL表示占位符
        
        ADD field1 value1 field2 value2 ...;#在表末尾添加一行数据，数据按照字段顺序依次给出，要求NOTNULL字段必须给出数据
        
        INSERT (line1_data,NULL,...) AT line_number; #在指定行号处插入一行数据，line_number从0开始计数
        
        UPDATE line_number (field1_name = value1, field2_name = value2,...); #更新指定行号的部分数据
        
        DEL line_number; #删除指定行号的行数据
        
        DEL COORDINATE x1 y1 x2 y2...; #删除指定坐标范围内数据
        
        SWAP line_number1 line_number2; #交换两行数据
        
        #GET简单查询操作
        
        GET FIELD field_name;#获取指定字段对应的列的数据
        
        GET line_number1 line_number2 ...; #获取指定行号的多行数据
        
        GET COORDINATE x1 y1 x2 y2 ...; #获取指定坐标范围内的多行数据
        
        GET ALL; #获取所有数据
        
        #SELECT复杂查询操作
        
        #其它命令
        
        DESC table_name;#查看指定表表结构
        
        DESC;#查看所在表结构
    
    }

##### 下面是操作对象为KVALOT时的所有语法@MHU

    关键字【KVALOT】【HOOK】【SET,GET,DEL,INCR,DECR,EXISTS,SELECT,ALL,APPEND,FROM,KEY,TYPE,LEN】
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

##### 下面是操作对象为STREAM时的所有语法@MHU
    
    关键字【STREAM】【HOOK】【ADD,GET,RANGE,DEL】
    
    {
        
        HOOK STREAM mystream; #创建一个名为mystream的流对象，此时操作对象为mystream
        
        APPEND value; #向流中附加数据
        
        APPEND value pos;# 将value追加到流中，从指定位置开始。若pos超出流的长度，则从流的末尾开始追加。
        
        GET FROM start end;#获取流中指定范围的数据
        
        GET pos len;#获取流中指定长度的数据
        
        SET pos value;# 从pos处设置流中指定位置的值
        
        SET char FROM start TO end;# 从start到end处设置流中指定范围的值
    
    }
    
##### 下面是操作对象为LIST时的所有语法@MHU
    
    关键字【LIST】【HOOK】【ADD,GET,DEL,LEN,INSERT,UPDATE,LPUSH,RPUSH,LPOP,RPOP,FROM,ALL,TO,WHOIS,AT,SET,EXISTS】
    
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


##### 下面是操作对象为BITMAP时的所有语法@MHU
    
    关键字【BITMAP】【HOOK】【SETBIT,GETBIT,COUNT,BITOP】
    
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
