/*
#版权所有 (c) HuJi 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define end (str){NULL,0}
#define err -1

typedef struct str {
    uint8_t *string;//STREAM:字节流的长度
    uint32_t len;//字节流的长度
} str;

str stostr(uint8_t *string, uint32_t len)
{
    /*
    获得一个字节流对象指针,内容为string,长度为len
    stostr:将C字符串s变成to为str

    返回的str不接管C字符串的所有权
    */
    str stream;
    stream.string = (uint8_t*)malloc(len);
    if (stream.string == NULL) {
        stream.len = 0;
        return stream;
    }
    memcpy(stream.string, string, len);
    stream.len = len;
    return stream;
}
void sfree(str* stream,...) 
{
    /*
    释放一个字节流对象
    sfree(&stream0,&stream1,&stream2,&stream3,...,NULL);
    */
    va_list args;//定义一个va_list类型的变量args
    va_start(args,stream);//初始化args,准备开始获取可变参数

    free(stream->string);
    stream->string = NULL;
    stream->len = 0;

    for(;;){
        str* temp = va_arg(args,str*);//获取可变参数
        if(temp == NULL || temp->string == NULL || temp->len == 0){
            break;
        }
        free(temp->string);
        temp->string = NULL;
        temp->len = 0;
    }
    va_end(args);//arg置为NULL
    return;
}
int sread(str* stream, uint32_t pos, uint8_t *bitestream, uint32_t length)
{
    //从pos开始读取length个字节到bitestream中
    if (stream->string == NULL || bitestream == NULL || pos >= stream->len) 
    {
        return err;
    }
    // 计算实际要读取的长度
    uint32_t available = stream->len - pos;
    uint32_t read_length = (length < available) ? length : available;
    // 复制数据到bitestream
    memcpy(bitestream, stream->string + pos, read_length);
    return read_length;
}
int swrite(str* stream, uint32_t pos, uint8_t *bitestream, uint32_t length)
{
    if (stream->string == NULL || bitestream == NULL || length == 0) {
        return err;
    }
    if (pos > stream->len) {
        pos = stream->len; // 如果位置超出范围，则追加到末尾
    }
    // 如果超范围，则扩展内存以容纳新数据
    if (pos + length > stream->len) {
        uint8_t *new_string = (uint8_t *)realloc(stream->string, pos + length);
        if (new_string == NULL) {
            return err;
        }
        stream->string = new_string;
        stream->len = pos + length;
    }
    // 将新数据复制到指定位置
    memcpy(stream->string + pos, bitestream, length);
    stream->len = pos + length;//更新长度
    return pos;
}
str splice(str stream0,...)
{
    /*
    将多个字节流对象拼接成一个字节流对象
    splice(stream0,stream1,stream2,stream3,...,end);
    返回一个字节流对象
    */
    va_list args;//定义一个va_list类型的变量args
    va_start(args,stream0);//初始化args,准备开始获取可变参数 
    
    str splice_stream = stostr(stream0.string, stream0.len);

    for(;;){
        str temp = va_arg(args,str);//获取可变参数
        if(temp.string == NULL && temp.len == 0){
            break;
        }
        swrite(&splice_stream,splice_stream.len,temp.string,temp.len);
    }   

    va_end(args);//arg置为NULL
    return splice_stream;
}
uint8_t* strtos(str stream)
{
    swrite(&stream,stream.len,(uint8_t*)"\0",1);
    return (uint8_t*)stream.string;
}
/*
int main()
{
    str mystr = stostr("hello",5);
    str mystr2 = stostr("world",5);
    str mystr3 = stostr("!",1);
    str mystr4 = stostr(" ",1);
    str mystr5 = splice(mystr,mystr4,mystr2,mystr3,end);
    printf("%s#",strtos(mystr5));

    sfree(&mystr,&mystr2,&mystr3,&mystr4,&mystr5,end);
}
*/