#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define merr  -1

/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/
/*
2025.1.17:逐位查找效率太低，改为逐字节查找，同时降低最大偏移量为2^32-1（原来是2^64-1) huji
2025.3.17:全面C++化,增强bitmap的功能性,同时提供valid()方法,并取消其它方法的一切安全检查,全面为效率服务
BITMAP 结构
0~3             bit数
4~(bit/8+7)     数据区大小

4字节对齐
*/

// 位拷贝函数：从源地址指定位置开始，拷贝指定位数到目标地址指定位置
// destination: 目标地址起始指针
// dest_first_bit: 目标地址起始位（从0开始计数）
// source: 源地址起始指针
// source_first_bit: 源地址起始位（从0开始计数）
// len: 需要拷贝的位数
extern"C" void bitcpy(uint8_t* destination, uint8_t dest_first_bit,const uint8_t* source, uint8_t source_first_bit,uint32_t len) {
    //该代码安全性尚未得到保证
    if (!len) return;
    #define MIN(a, b) ((a) < (b) ? (a) : (b))

    // 计算源和目标的起始字节及位偏移
    uint32_t s_byte = source_first_bit / 8;
    uint8_t s_bit_in_byte = source_first_bit % 8;
    uint32_t d_byte = dest_first_bit / 8;
    uint8_t d_bit_in_byte = dest_first_bit % 8;

    // 计算源和目标的结束位
    uint32_t s_end_bit = source_first_bit + len - 1;
    uint32_t s_end_byte = s_end_bit / 8;
    uint8_t s_end_bit_in_byte = s_end_bit % 8;

    // 计算目标的结束位
    uint32_t d_end_bit = dest_first_bit + len - 1;
    uint32_t d_end_byte = d_end_bit / 8;
    uint8_t d_end_bit_in_byte = d_end_bit % 8;

    // 处理源和目标在同一个字节的情况
    if (s_byte == s_end_byte && d_byte == d_end_byte) {
        uint8_t src_val = (source[s_byte] >> s_bit_in_byte) & 
                          ((1 << (s_end_bit_in_byte - s_bit_in_byte + 1)) - 1);
        uint8_t mask = ((1 << (d_end_bit_in_byte - d_bit_in_byte + 1)) - 1) << d_bit_in_byte;
        destination[d_byte] = (destination[d_byte] & ~mask) | (src_val << d_bit_in_byte);
        return;
    }

    // 处理头部（源的起始字节到第一个完整字节）
    uint8_t a = 8 - s_bit_in_byte; // 源起始字节剩余位数
    uint8_t b = 8 - d_bit_in_byte; // 目标起始字节剩余位数
    uint8_t src_part = (source[s_byte] >> s_bit_in_byte) & ((1 << a) - 1);
    uint8_t *dest_ptr = destination + d_byte;

    if (a <= b) {
        uint8_t mask = ((1 << a) - 1) << d_bit_in_byte;
        *dest_ptr = (*dest_ptr & ~mask) | (src_part << d_bit_in_byte);
    } else {
        // 部分复制到目标起始字节，剩余到下一个字节
        uint8_t mask = ((1 << b) - 1) << d_bit_in_byte;
        *dest_ptr = (*dest_ptr & ~mask) | ((src_part & ((1 << b) - 1)) << d_bit_in_byte);
        dest_ptr[1] = (dest_ptr[1] & ~(((1 << (a - b)) - 1) << 0)) | ((src_part >> b) & ((1 << (a - b)) - 1));
    }

    // 处理中间完整字节块
    uint32_t mid_bytes = s_end_byte - s_byte - 1;
    if (mid_bytes > 0) {
        const uint8_t *src_mid = source + s_byte + 1;
        uint8_t *dest_mid = destination + d_byte + 1;
        int delta = (d_bit_in_byte - s_bit_in_byte) % 8;
        if (delta < 0) delta += 8;
        if (delta == 0) {
            // 位偏移相同，直接复制
            memcpy(dest_mid, src_mid, mid_bytes);
        } 
        else {
            // 逐字节调整位偏移
            for (uint32_t i = 0; i < mid_bytes; i++) {
                uint8_t src_byte = src_mid[i];
                uint8_t shifted = (src_byte << delta) & 0xFF;
                uint8_t mask = (0xFF << d_bit_in_byte) & 0xFF;
                dest_mid[i] = (dest_mid[i] & ~mask) | (shifted & mask);
            }
        }
    }

    // 处理尾部（源的最后一个字节到结束位）
    uint8_t src_last = source[s_end_byte] & ((1 << (s_end_bit_in_byte + 1)) - 1);
    uint8_t *dest_end = destination + d_end_byte;

    // 计算需要复制的位数
    uint8_t src_available_bits = s_end_bit_in_byte + 1;
    uint8_t dest_available_bits = d_end_bit_in_byte + 1;
    uint8_t copy_bits = MIN(src_available_bits, dest_available_bits);

    // 计算 mask 和偏移量
    uint8_t mask = ((1 << copy_bits) - 1) << (d_end_bit_in_byte - copy_bits + 1);
    *dest_end = (*dest_end & ~mask) | ((src_last << (d_end_bit_in_byte - copy_bits + 1)) & mask);

    // 处理跨字节尾部
    if (copy_bits < src_available_bits) {
        uint8_t remaining_bits = src_available_bits - copy_bits;
        uint8_t remaining_src = (src_last >> copy_bits) & ((1 << remaining_bits) - 1);
        dest_end[1] = (dest_end[1] & ~((1 << remaining_bits) - 1)) | remaining_src;
    }
    #undef MIN // 结束宏的作用域
}

#define bitmap_debug

/*
* 
*   BITMAP (c) HUJI 2025.3
*   位图
*   该类提供了一个位图的基本功能,可以用于存储和操作二进制数据。//在bitmap_debug宏被定义的情况下，BITMAP会直接输出错误信息。
*   该类的基本功能和注意事项如下:
*       1. 初始化方法:
*           <1> BITMAP(uint32_t size): 
*               初始化一个大小为size的位图,size的单位为bit。注意size的大小不能超过堆区的大小。如果超过堆区的大小,
*               BITMAP会自动尝试初始化为size=0的位图，这对后续操作可能有影响，比如后续调用valid函数会抛出异常。
*           <2> BITMAP(BITMAP& otherbitmap):
*               初始化一个与otherbitmap相同的位图。确保otherbitmap是有效的，如果无效，BITMAP会尝试初始化为size=0，
*               这可能会影响后续操作。
*           <3> BITMAP(char* s,uint32_t len,uint8_t zerochar):
*               初始化一个大小为len的位图,并将s中的数据复制到位图中,如果s中的字符等于zerochar,则将对应位置的bit设置为0,
*               否则设置为1。请确保s是有效的,否则可能会导致未定义行为。
*       2. 赋值操作:
*           <1> BITMAP& operator=(BITMAP& otherbitmap):
*               将otherbitmap赋值给当前的位图。确保otherbitmap是有效的，如果无效，当前位图会保留原始数据，但抛出state异常。
*           <2> BITMAP& operator=(uint8_t* otherbitmap):
*               这个函数不能直接使用，直接继承指针，这个函数只会在下面情况下被隐式调用：bitmap = bitmap1 + bitmap2;
*               通过operator+将两个bitmap相加，返回指针，然后将指针赋值给bitmap。
*
*
*
*
*
*
*/
class BITMAP  {
    private:
        uint8_t* bitmap;
        int state = 0;//对象状态
        int rexpand(uint32_t size);//该容函数将bitmap的大小改变到size,执行失败返回merr
    public:
        BITMAP(uint32_t size){
            this->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint32_t),1);//初始化一个size位的bitmap,bitmap的大小：(size+7)/8 + 8 = (size + 15)/8
            if(this->bitmap == NULL){
                this->bitmap = (uint8_t*)calloc(4,1);
                #ifdef bitmap_debug
                perror("BITMAP memmery init failed!");
                #endif
                state++;return;//初始化失败
            }
            *(uint32_t*)this->bitmap = size;
            return;
        }
        BITMAP(BITMAP& otherbitmap){
            if(otherbitmap.bitmap == NULL){
                this->bitmap = (uint8_t*)calloc(4,1);
                #ifdef bitmap_debug
                perror("BITMAP failed!otherbitmap is NULL!");
                #endif
                state++;return;
            }
            uint32_t size = *(uint32_t*)otherbitmap.bitmap;
            this->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint32_t),1);
            if(this->bitmap == NULL){
                this->bitmap = (uint8_t*)calloc(4,1);
                #ifdef bitmap_debug
                perror("BITMAP memmery init failed!");
                #endif
                state++;return;
            }
            memcpy(this->bitmap,otherbitmap.bitmap,(size+7)/8 + sizeof(uint32_t));
            return;
        }
        BITMAP(char* s,uint32_t len,uint8_t zerochar){
            this->bitmap = (uint8_t*)calloc((len+7)/8 + sizeof(uint32_t),1);
            if(this->bitmap == NULL){
                #ifdef bitmap_debug
                perror("BITMAP memmery init failed!");
                #endif
                state++;return;
            }
            *(uint32_t*)this->bitmap = len;
            uint8_t* first = this->bitmap + sizeof(uint32_t); // 指向数据区
            for(uint32_t i=0;i<len;i++){
                if(s[i] != zerochar) first[i/8] |= 1 << (i%8); // 将该位置为1
            }
            return;
        }
        ~BITMAP(){free(bitmap);}
        BITMAP& operator=(BITMAP otherbitmap){
            uint8_t* temp=this->bitmap;
            if(otherbitmap.bitmap == NULL){
                #ifdef bitmap_debug
                perror("BITMAP failed!otherbitmap is NULL!");
                #endif
                this->bitmap = temp;
                state++;return *this;
            }
            uint32_t size = *(uint32_t*)otherbitmap.bitmap;
            this->bitmap = (uint8_t*)malloc((size+7)/8 + sizeof(uint32_t));
            if(this->bitmap == NULL){
                #ifdef bitmap_debug
                perror("BITMAP memmery init failed!");
                #endif
                this->bitmap = temp;
                state++;return *this;
            }
            memcpy(this->bitmap,otherbitmap.bitmap,(size+7)/8 + sizeof(uint32_t));
            free(temp);
            return *this;
        }
        BITMAP& operator+=(BITMAP& otherbitmap){
            if(otherbitmap.bitmap == NULL){
                #ifdef bitmap_debug
                perror("BITMAP failed!otherbitmap is NULL!");
                #endif
                state++;return *this;
            }
            uint32_t size = *(uint32_t*)this->bitmap + *(uint32_t*)otherbitmap.bitmap;
            uint8_t* temp = this->bitmap;
            this->bitmap = (uint8_t*)malloc((size+7)/8 + sizeof(uint32_t));
            if(this->bitmap == NULL){
                #ifdef bitmap_debug
                perror("BITMAP memmery init failed!");
                #endif
                this->bitmap = temp;
                state++;return *this;
            }
            *(uint32_t*)this->bitmap = size;
            bitcpy(this->bitmap+sizeof(uint32_t),0,temp+sizeof(uint32_t),0,*(uint32_t*)temp);
            bitcpy(this->bitmap+sizeof(uint32_t),*(uint32_t*)temp,otherbitmap.bitmap+sizeof(uint32_t),0,*(uint32_t*)otherbitmap.bitmap);
            free(temp);
            return *this;
        }
        class BIT{//代理类
            private:
                BITMAP& mybitmap;
                uint32_t offset;
            public:
                BIT(BITMAP& bitmap,uint32_t offset):mybitmap(bitmap),offset(offset){}
                BIT& operator=(int value){//调用前确保this->bitmap != NULL && offset < *(uint32_t*)this->bitmap
                    uint8_t* first = this->mybitmap.bitmap + sizeof(uint32_t); // 指向数据区
                    if(value){
                        first[offset/8] |= 1 << (offset%8); // 将该位置为1
                        return *this;
                    }                    
                    first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
                    return *this; 
                }
                operator int(){
                    return ( (mybitmap.bitmap+sizeof(uint32_t))[offset/8] >> (offset%8) ) & 1;
                }
                operator int() const{
                    return ( (mybitmap.bitmap+sizeof(uint32_t))[offset/8] >> (offset%8) ) & 1;
                }
        };
        BIT operator[](uint32_t offset){
            if(offset >= *(uint32_t*)this->bitmap && this->rexpand(offset+1)==merr) state++;
            return BIT(*this,offset);
        }
        
        int valid();
        int set(uint32_t offset,uint8_t value);
        int set(uint32_t offset, uint32_t len, uint8_t value);
        int set(uint32_t offset,uint32_t len,const char* data_stream,char zero_value);
        uint32_t size();
        uint32_t count(uint32_t st_offset,uint32_t ed_offset);
};
inline int BITMAP::rexpand(uint32_t size) {
    uint32_t old_bite_num = (*(uint32_t*)this->bitmap + 7)/8 + sizeof(uint32_t);
    uint32_t new_bite_num = (size + 7)/8 + sizeof(uint32_t);
    
    if (old_bite_num >= new_bite_num) {
        // 缩小内存realloc一般不会失败 更新尺寸字段
        bitmap = (uint8_t*)realloc(bitmap, new_bite_num);
        *(uint32_t*)bitmap = size;
        return 0;
    }
    
    uint8_t* new_bitmap = (uint8_t*)realloc(bitmap, new_bite_num);
    if (new_bitmap) {
        bitmap = new_bitmap;
        uint32_t old_size = *(uint32_t*)bitmap;//备份原始尺寸
        *(uint32_t*)bitmap = size;
        set(old_size,size-old_size,0);
        return 0;
    }
    return merr; // 内存分配失败
}
inline int BITMAP::set(uint32_t offset, uint32_t len, uint8_t value) {
    if(!len)return 0;
    if(offset + len > *(uint32_t*)bitmap && this->rexpand(offset + len)==merr) return merr;

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
inline int BITMAP::set(uint32_t offset,uint32_t len,const char* data_stream,char zero_value){
    ///data_stream是一个长度必须大于等于len的C字符串，里面的字符是zero_value或'其他字符',对于zero_value：置0  其他：置1
    if( len == 0 || data_stream == NULL || (offset + len > *(uint32_t*)bitmap && this->rexpand(offset + len)==merr ) ){
        return merr;
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
inline int BITMAP::set(uint32_t offset,uint8_t value){
    if(offset >= *(uint32_t*)this->bitmap && this->rexpand(offset+1)==merr ) {        
        return merr; //越界,自动扩容
    }
    if(value){//value=0 ：置0,  否则 ：置1
        (bitmap + sizeof(uint32_t))[offset/8] |= 1 << (offset%8); // 将该位置为1
        return 0;
    }
    (bitmap + sizeof(uint32_t))[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
    return 0;
}
inline uint32_t BITMAP::size(){
    return *(uint32_t*)this->bitmap;
}
inline int BITMAP::valid(){
    /*
    内存检查:
    1. bitmap是否为NULL
    2. bitmap的内存重申请
    状态检查:
    */
    if( this->bitmap==NULL || (this->bitmap = (uint8_t*)realloc(this->bitmap,(*(uint32_t*)this->bitmap + 7)/8 + sizeof(uint32_t))) == NULL ){
        return merr;
    }
    return 0;
}
inline uint32_t BITMAP::count(uint32_t st_offset,uint32_t ed_offset)//包括start和end,数1的个数
{
    if(bitmap == NULL || (st_offset > ed_offset || ed_offset >= *(uint32_t*)bitmap || st_offset >= *(uint32_t*)bitmap)){
        return merr;
    }
   
    uint8_t* first = bitmap + sizeof(uint32_t); // 指向数据区
    uint32_t sum = 0; // 初始化计数器
    for(;st_offset <= ed_offset;st_offset++){
        if(first[st_offset/8] & (1 << (st_offset%8)))sum++; // 如果该位为1，计数器加1
    }
    return sum; // 返回计数结果
}



void ptf(BITMAP& bitmap){//测试用函数
    for(uint32_t i=0;i<bitmap.size();i++){
        printf("%d",(int)bitmap[i]);
    }
    printf("\n");
    return;
}

int main(){
    BITMAP bitmap(5);
    bitmap[0] = 1;
    bitmap[1] = 0;
    bitmap[2] = 1;
    bitmap[3] = 0;
    bitmap[4] = 1;
    ptf(bitmap);//10101
    BITMAP bitmap1(5);
    //bitmap1[0] = bitmap[0] ;//报错？
    bitmap1[0] = bitmap[0]+bitmap[1];
    bitmap1[1] = 3;
    bitmap1[4] = 2;
    //11001
    BITMAP bitmap2(3);
    bitmap1 += bitmap;//报错
    bitmap2 = bitmap1;
    ptf(bitmap2);
}


/*
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