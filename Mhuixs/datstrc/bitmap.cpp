#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define merr  -1


/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
2025.1.17:逐位查找效率太低，改为逐字节查找，同时降低最大偏移量为2^32-1（原来是2^64-1) huji
*/
/*
BITMAP 结构
0~3             bit数
4~(bit/8+7)     数据区大小
*/
/*
4字节对齐
*/

class BITMAP{
    private:
        uint8_t* bitmap;
        //该容函数将bitmap的大小改变到size,执行失败返回merr
        int rexpand(uint32_t size) {
            uint32_t old_bite_num = (*(uint32_t*)bitmap + 7)/8 + sizeof(uint32_t);
            uint32_t new_bite_num = (size + 7)/8 + sizeof(uint32_t);

            if (old_bite_num >= new_bite_num) {
                // 缩小内存realloc一般不会失败 更新尺寸字段
                bitmap = (uint8_t*)realloc(bitmap, new_bite_num);
                *(uint32_t*)bitmap = size;
                return 0;
            }
        
            uint8_t* new_bitmap = (uint8_t*)realloc(bitmap, new_bite_num);
            if (!new_bitmap) {
                return merr; // 内存分配失败
            }
            bitmap = new_bitmap;
            uint32_t old_size = *(uint32_t*)bitmap;//备份原始尺寸
            *(uint32_t*)bitmap = size;
            set(old_size,size-old_size,0);
            return 0;
        }
    public:
        BITMAP(uint32_t size){
            /*
            初始化一个size位的bitmap
            */
            //bitmap的大小：(size+7)/8 + 8 = (size + 15)/8
            this->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint32_t),1);
            if(this->bitmap == NULL){
                return;
            }
            *(uint32_t*)this->bitmap = size;
            return;
        }
        BITMAP(BITMAP& otherbitmap){
            uint32_t size = *(uint32_t*)otherbitmap.bitmap;
            this->bitmap = (uint8_t*)calloc(size,1);
            if(this->bitmap == NULL){
                return;
            }
            memcpy(this->bitmap,otherbitmap.bitmap,size);
            return; 
        }
        BITMAP(char* s,uint32_t len){
            this->bitmap = (uint8_t*)calloc((len+7)/8 + sizeof(uint32_t),1);
            if(this->bitmap == NULL){
                return;          
            }
            *(uint32_t*)this->bitmap = len;
            for(uint32_t i=0;i<len;i++){
                if(s[i] != '0') {
                    uint8_t* first = this->bitmap + sizeof(uint32_t); // 指向数据区
                    first[i/8] |= 1 << (i%8); // 将该位置为1
                }
            }
            return;
        }
        ~BITMAP(){
            free(bitmap);
        }
        int operator[](uint32_t offset) const {//const 修饰的成员函数不能修改成员变量的值
            if(this->bitmap == NULL || offset >= *(uint32_t*)this->bitmap){
                return merr;
            }
            
            return ( (this->bitmap+sizeof(uint32_t))[offset/8] >> (offset%8) ) & 1; // 获取指定位置的bit值 
            //内存中序号和位的对应关系：7-6-5-4-3-2-1-0 15-14-13-12-11-10-9-8
        }
        BITMAP& operator=(BITMAP& otherbitmap){
            uint8_t* temp=this->bitmap;
            uint32_t size = *(uint32_t*)otherbitmap.bitmap;
            this->bitmap = (uint8_t*)malloc(size);
            if(this->bitmap == NULL){
               this->bitmap = temp;
               return *this;
            }
            memcpy(this->bitmap,otherbitmap.bitmap,size);
            free(temp);
            return *this;
        }
        class BIT{//代理类
            private:
                BITMAP& mybitmap;
                uint32_t offset;
            public:
                BIT(BITMAP& bitmap,uint32_t offset):mybitmap(bitmap),offset(offset){}

                BIT& operator=(bool value){
                    //调用前确保this->bitmap != NULL ||offset < *(uint32_t*)this->bitmap
                    uint8_t* first = this->mybitmap.bitmap + sizeof(uint32_t); // 指向数据区
                    if(value == 0){
                        first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
                        return *this; 
                    }
                    first[offset/8] |= 1 << (offset%8); // 将该位置为1
                    return *this;
                }
                operator bool(){
                    return ( (mybitmap.bitmap+sizeof(uint32_t))[offset/8] >> (offset%8) ) & 1;
                    //return (bool)mybitmap[offset];
                }
        };
        
        BIT operator[](uint32_t offset){
            if(this->bitmap == NULL ){
                printf("bitmap class err with no expection!\n");
                system("pause");
                exit(1);
            }
            if(offset >= *(uint32_t*)this->bitmap){
                //越界,自动扩容
                this->rexpand(offset+1);
            }
            return BIT(*this,offset); 
        }
        int set(uint32_t offset){//置1 
            if(this->bitmap == NULL ){
                return merr;
            }
            if(offset >= *(uint32_t*)this->bitmap){
                //越界,自动扩容
                this->rexpand(offset+1);
            }
            uint8_t* first = this->bitmap + sizeof(uint32_t); // 指向数据区
            first[offset/8] |= 1 << (offset%8); // 将该位置为1
            return 0; // 成功设置，返回0
        }
        int set(uint32_t offset,uint8_t value){//value=0 ：置0,  否则 ：置1
            if(this->bitmap == NULL ){
                return merr;
            }
            if(offset >= *(uint32_t*)this->bitmap){
                //越界,自动扩容
                this->rexpand(offset+1);
            }
            uint8_t* first = bitmap + sizeof(uint32_t); // 指向数据区
            if(value == 0){
                first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
                return 0;
            }
            first[offset/8] |= 1 << (offset%8); // 将该位置为1
            return 0;    
        }
        int set(uint32_t offset, uint32_t len, uint8_t value) {
            if(this->bitmap == NULL || len == 0 ){
                return merr;
            }
            if ( offset + len > *(uint32_t*)bitmap) {
                //越界,自动扩容
                this->rexpand(offset + len);
            }
        
            uint8_t* data = bitmap + sizeof(uint32_t);
            uint32_t s_byte = offset / 8;
            uint32_t e_bit = offset + len - 1;
            uint32_t e_byte = e_bit / 8;
            uint32_t s_bit = offset % 8;
            uint32_t e_bit_in_byte = e_bit % 8;
        
            if (s_byte == e_byte) {
                // 同一字节处理
                uint8_t mask = ((1 << (e_bit_in_byte + 1)) - 1) & ~((1 << s_bit) - 1);
                data[s_byte] = value ? (data[s_byte] | mask) : (data[s_byte] & ~mask);
                return 0;
            }
        
            // 处理头部：起始字节的s_bit到末尾
            uint8_t mask_head = (0xFF << s_bit) & 0xFF;
            data[s_byte] = value ? (data[s_byte] | mask_head) : (data[s_byte] & ~mask_head);
        
            // 处理中间完整字节
            uint32_t mid_bytes = e_byte - s_byte - 1;
            if (mid_bytes > 0) {
                memset(data + s_byte + 1, value ? 0xFF : 0, mid_bytes);
            }
        
            // 处理尾部：结束字节的0到e_bit_in_byte
            uint8_t mask_tail = (1 << (e_bit_in_byte + 1)) - 1;
            data[e_byte] = value ? (data[e_byte] | mask_tail) : (data[e_byte] & ~mask_tail);
        
            return 0;
        }
        int set(uint32_t offset,uint32_t len,const char* data_stream,char zero_value)//包括start和end
        {
            /*
            data_stream是一个字符串，里面的字符是zero_value或'其他字符'
            data_stream的长度必须大于等于end-start+1
            zero_value：置0
            其他：置1
            */
            if(this->bitmap == NULL || len == 0 || data_stream == NULL ){
                return merr;
            }
            if(offset + len > *(uint32_t*)bitmap ){
                this->rexpand(offset + len);
            }
            uint8_t* first = bitmap + sizeof(uint32_t); // 指向数据区
            for(uint32_t i=0;i<len;i++,offset++){
                if(data_stream[i] == zero_value){
                    first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
                    continue;
                }
                first[offset/8] |= 1 << (offset%8); // 将该位置为1
            }
            return 0; // 成功设置，返回0
        }

};


/*
int64_t countBIT(BITMAP* bitmap,uint32_t start,uint32_t end)//包括start和end,数1的个数
{
    if(bitmap == NULL){
        return merr;
    }
    if(start > end || end >= *(uint32_t*)bitmap || start >= *(uint32_t*)bitmap){
        return merr;
    }
    uint8_t* first = bitmap + sizeof(uint32_t); // 指向数据区
    uint32_t num = 0; // 初始化计数器
    for(;start <= end;start++){
        if(first[start/8] & (1 << (start%8))){
            num++; // 如果该位为1，计数器加1
        }
    }
    return num; // 返回计数结果(int64_t)
}
int64_t retuoffset(BITMAP* bitmap,uint32_t start,uint32_t end)//包括start和end,返回范围内第一个1的位置
{
    if(bitmap == NULL){
        return merr;
    }
    if(start > end || end >= *(uint32_t*)bitmap || start >= *(uint32_t*)bitmap){
        return merr;
    }
    uint8_t* first = bitmap + sizeof(uint32_t); // 指向数据区
    if(end-start < 32){ 
        //如果范围小于32，则使用逐位查找
        for(;start <= end;start++){
            if(first[start/8] & (1 << (start%8))){
            return start; // 返回第一个1的位置
            }
        }
        return merr; // 如果没有找到，则返回err
    }
    //如果范围大于32，则使用逐字节查找
    //先找start所在的字节
    int st_bit = start%8;
    for(;st_bit < 8;st_bit++){
        if(first[start/8] & (1 << st_bit)){//1000 0000 >> st_byte            
            return start + st_bit;// 返回第一个1的位置
        }
    }
    //再找中间的字节
    uint32_t st_byte = start/8+1;//开始字节
    uint32_t en_byte = end/8-1;//结束字节
    for(;st_byte <= en_byte;st_byte++){
        if(first[st_byte]!= 0){
            for(uint8_t i = 0;i < 8;i++){
                if(first[st_byte] & (1 << i)){
                    return st_byte*8 + i; // 返回第一个1的位置
                }
            }
        }
    }
    //再找end所在的字节
    int en_bit = end%8;
    for(uint8_t i = 0;i <= en_bit;i++){
        if(first[end/8] & (1 << i)){
            return end - en_bit + i; // 返回第一个1的位置
        }
    }
    return merr; // 如果没有找到，则返回err
}

void printBITMAP(BITMAP* bitmap)
{
    if(bitmap == NULL){
        return;
    }
    uint64_t size = *(uint64_t*)bitmap; // 获取bitmap的大小
    for(uint64_t i = 0;i < size;i++){
        printf("%d",getBIT(bitmap,i)); // 打印每个bit的值
    }
    printf("\n"); // 默认换行
}
    */

    #include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>



int main() {
    // 测试1: 初始化一个BITMAP对象
    BITMAP bitmap(10); // 创建一个10位的bitmap
    std::cout << "Test 1: Initialization of a 10-bit bitmap" << std::endl;
    for (int i = 0; i < 10; ++i) {
        std::cout << bitmap[i] << " "; // 输出初始化的bitmap，应该全为0
    }
    std::cout << std::endl << std::endl;

    // 测试2: 设置某些位为1
    std::cout << "Test 2: Setting some bits to 1" << std::endl;
    bitmap.set(2);
    bitmap.set(5);
    bitmap.set(8);
    for (int i = 0; i < 10; ++i) {
        std::cout << bitmap[i] << " "; // 输出bitmap，第2、5、8位应该为1
    }
    std::cout << std::endl << std::endl;

    // 测试3: 使用代理类BIT进行位操作
    std::cout << "Test 3: Using BIT proxy class to manipulate bits" << std::endl;
    bitmap[1] = true;
    bitmap[3] = true;
    bitmap[7] = false; // 第7位应该保持为0
    for (int i = 0; i < 10; ++i) {
        std::cout << bitmap[i] << " "; // 输出bitmap，第1、2、3、5、8位应该为1
    }
    std::cout << std::endl << std::endl;

    // 测试4: 自动扩容
    std::cout << "Test 4: Automatic expansion" << std::endl;
    bitmap[15] = true; // 访问第15位，触发自动扩容
    for (int i = 0; i < 16; ++i) {
        std::cout << bitmap[i] << " "; // 输出bitmap，第1、2、3、5、8、15位应该为1
    }
    std::cout << std::endl << std::endl;

    // 测试5: 拷贝构造函数
    std::cout << "Test 5: Copy constructor" << std::endl;
    BITMAP bitmap2 = bitmap; // 使用拷贝构造函数
    for (int i = 0; i < 16; ++i) {
        std::cout << bitmap2[i] << " "; // 输出bitmap2，应该与bitmap相同
    }
    std::cout << std::endl << std::endl;

    // 测试6: 赋值运算符
    std::cout << "Test 6: Assignment operator" << std::endl;
    BITMAP bitmap3(5); // 创建一个5位的bitmap
    bitmap3 = bitmap; // 使用赋值运算符
    for (int i = 0; i < 16; ++i) {
        std::cout << bitmap3[i] << " "; // 输出bitmap3，应该与bitmap相同
    }
    std::cout << std::endl << std::endl;

    // 测试7: 使用字符串初始化BITMAP
    std::cout << "Test 7: Initialization with a string" << std::endl;
    char data[] = "1010101010";
    BITMAP bitmap4(data, 10); // 使用字符串初始化bitmap
    for (int i = 0; i < 10; ++i) {
        std::cout << bitmap4[i] << " "; // 输出bitmap4，应该与字符串对应
    }
    std::cout << std::endl << std::endl;

    // 测试8: 批量设置位
    std::cout << "Test 8: Batch setting bits" << std::endl;
    char data_stream[] = "000111000111";
    bitmap4.set(0, 12, data_stream, '0'); // 批量设置位
    for (int i = 0; i < 12; ++i) {
        std::cout << bitmap4[i] << " "; // 输出bitmap4，应该与data_stream对应
    }
    std::cout << std::endl << std::endl;

    return 0;
}