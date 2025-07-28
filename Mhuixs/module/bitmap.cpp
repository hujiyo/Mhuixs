#include "bitmap.hpp"
#include "nlohmann/json.hpp"
/*
#版权所有 (c) HUJI 2025
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

/*
BITMAP 结构
0~7             bit数
8~(bit/8+15)    数据区大小

8字节对齐
*/

BITMAP::BITMAP():bitmap((uint8_t*)calloc(8,1)){
    if(this->bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        state++;
        return;
    }
}

BITMAP::BITMAP(const char* s){
    uint64_t len = strlen(s);
    this->bitmap = (uint8_t*)malloc(8+len/8+1);
    if (!this->bitmap) {
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        state++;return; 
    }
    *(uint64_t*)this->bitmap = len;
    for(uint64_t i = 0;i < len;i++){
        if (s[i] == '0') set(i,0);
        else set(i,1); 
    }
}


BITMAP::BITMAP(uint64_t size){
    this->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint64_t),1);//初始化一个size位的bitmap,bitmap的大小：(size+7)/8 + 8 = (size + 15)/8
    if(this->bitmap == NULL){
        this->bitmap = (uint8_t*)calloc(8,1);
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        state++;return;//初始化失败
    }
    *(uint64_t*)this->bitmap = size;
    return;
}

BITMAP::BITMAP(const BITMAP& otherbitmap){
    if(otherbitmap.bitmap == NULL){
        this->bitmap = (uint8_t*)calloc(8,1);
        #ifdef bitmap_debug
        perror("BITMAP failed!otherbitmap is NULL!");
        #endif
        state++;return;
    }
    uint64_t size = *(uint64_t*)otherbitmap.bitmap;
    this->bitmap = (uint8_t*)calloc((size+7)/8 + sizeof(uint64_t),1);
    if(this->bitmap == NULL){
        this->bitmap = (uint8_t*)calloc(8,1);
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        state++;return;
    }
    memcpy(this->bitmap,otherbitmap.bitmap,(size+7)/8 + sizeof(uint64_t));
    return;
}

BITMAP::BITMAP(char* s,uint64_t len,uint8_t zerochar){
    this->bitmap = (uint8_t*)calloc((len+7)/8 + sizeof(uint64_t),1);
    if(this->bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        state++;return;
    }
    *(uint64_t*)this->bitmap = len;
    uint8_t* first = this->bitmap + sizeof(uint64_t); // 指向数据区
    for(uint64_t i=0;i<len;i++){
        if(s[i] != zerochar) first[i/8] |= 1 << (i%8); // 将该位置为1
    }
    return;
}

BITMAP::~BITMAP(){
    free(bitmap);
}
BITMAP& BITMAP::operator=(char* s){
    uint64_t len = strlen(s);
    uint8_t* temp=this->bitmap;
    this->bitmap = (uint8_t*)calloc((len+7)/8 + sizeof(uint64_t),1);
    if(this->bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        state++;return *this;
    }
    *(uint64_t*)this->bitmap = len;
    for(uint64_t i=0;i<len;i++){
        if(s[i] - '0') this->bitmap[i/8] |= 1 << (i%8);
        else this->bitmap[i/8] &= ~(1 << (i%8));
    }
    free(temp);
    return *this;
}
BITMAP& BITMAP::operator=(const BITMAP& otherbitmap){
    if(this == &otherbitmap) return *this; // 自赋值保护
    uint8_t* temp=this->bitmap;
    if(otherbitmap.bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP failed!otherbitmap is NULL!");
        #endif
        this->bitmap = temp;
        state++;return *this;
    }
    uint64_t size = *(uint64_t*)otherbitmap.bitmap;
    this->bitmap = (uint8_t*)malloc((size+7)/8 + sizeof(uint64_t));
    if(this->bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        this->bitmap = temp;
        state++;return *this;
    }
    memcpy(this->bitmap,otherbitmap.bitmap,(size+7)/8 + sizeof(uint64_t));
    free(temp);
    return *this;
}

BITMAP& BITMAP::operator+=(BITMAP& otherbitmap){
    if(otherbitmap.bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP failed!otherbitmap is NULL!");
        #endif
        state++;return *this;
    }
    uint64_t size = *(uint64_t*)this->bitmap + *(uint64_t*)otherbitmap.bitmap;
    uint8_t* temp = this->bitmap;
    this->bitmap = (uint8_t*)malloc((size+7)/8 + sizeof(uint64_t));
    if(this->bitmap == NULL){
        #ifdef bitmap_debug
        perror("BITMAP memmery init failed!");
        #endif
        this->bitmap = temp;
        state++;return *this;
    }
    *(uint64_t*)this->bitmap = size;
    bitcpy(this->bitmap+sizeof(uint64_t),0,temp+sizeof(uint64_t),0,*(uint64_t*)temp);
    bitcpy(this->bitmap+sizeof(uint64_t),*(uint64_t*)temp,otherbitmap.bitmap+sizeof(uint64_t),0,*(uint64_t*)otherbitmap.bitmap);
    free(temp);
    return *this;
}

class BITMAP::BIT{//代理类
    private:
        BITMAP& mybitmap;
        uint64_t offset;
    public:
        BIT(BITMAP& bitmap,uint64_t offset):mybitmap(bitmap),offset(offset){}
        BIT& operator=(int value){//调用前确保this->bitmap != NULL && offset < *(uint64_t*)this->bitmap
            uint8_t* first = this->mybitmap.bitmap + sizeof(uint64_t); // 指向数据区
            if(value){
                first[offset/8] |= 1 << (offset%8); // 将该位置为1
                return *this;
            }                    
            first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
            return *this; 
        }
        operator int(){
            return ( (mybitmap.bitmap+sizeof(uint64_t))[offset/8] >> (offset%8) ) & 1;
        }
        operator int() const{
            return ( (mybitmap.bitmap+sizeof(uint64_t))[offset/8] >> (offset%8) ) & 1;
        }
};

inline BITMAP::BIT BITMAP::operator[](uint64_t offset){
    if(offset >= *(uint64_t*)this->bitmap && this->rexpand(offset+1)==merr) state++;
    return BIT(*this,offset);
}

inline int BITMAP::rexpand(uint64_t size) {
    uint64_t old_bite_num = (*(uint64_t*)this->bitmap + 7)/8 + sizeof(uint64_t);
    uint64_t new_bite_num = (size + 7)/8 + sizeof(uint64_t);
    
    if (old_bite_num >= new_bite_num) {
        // 缩小内存realloc一般不会失败 更新尺寸字段
        bitmap = (uint8_t*)realloc(bitmap, new_bite_num);
        *(uint64_t*)bitmap = size;
        return 0;
    }
}

nlohmann::json BITMAP::get_all_info() const {
    nlohmann::json info;
    uint64_t s = size();
    info["size"] = s;
    std::string bitmap_str;
    bitmap_str.reserve(s);
    for (uint64_t i = 0; i < s; i++) {
        bitmap_str += ((*this)[i] ? '1' : '0');
    }
    info["bitmap"] = bitmap_str;
    return info;

    }
    
    uint8_t* new_bitmap = (uint8_t*)realloc(bitmap, new_bite_num);
    if (new_bitmap) {
        bitmap = new_bitmap;
        uint64_t old_size = *(uint64_t*)bitmap;//备份原始尺寸
        *(uint64_t*)bitmap = size;
        set(old_size,size-old_size,0);
        return 0;
    }
    return merr; // 内存分配失败
}

inline int BITMAP::set(uint64_t offset, uint64_t len, uint8_t value) {
    if(!len)return 0;
    if(offset + len > *(uint64_t*)bitmap && this->rexpand(offset + len)==merr) return merr;

    uint8_t* data = bitmap + sizeof(uint64_t);
    uint64_t s_byte = offset / 8;
    uint64_t e_bit = offset + len - 1;
    uint64_t e_byte = e_bit / 8;
    uint64_t s_bit = offset % 8;
    uint64_t e_bit_in_byte = e_bit % 8;

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
    uint64_t mid_bytes = e_byte - s_byte - 1;
    if (mid_bytes > 0) {
        memset(data + s_byte + 1, value ? 0xFF : 0, mid_bytes);
    }

    // 处理尾部：结束字节的0到e_bit_in_byte
    uint8_t mask_tail = (1 << (e_bit_in_byte + 1)) - 1;
    data[e_byte] = value ? (data[e_byte] | mask_tail) : (data[e_byte] & ~mask_tail);

    return 0;
}

inline int BITMAP::set(uint64_t offset,uint64_t len,const char* data_stream,char zero_value){
    ///data_stream是一个长度必须大于等于len的C字符串，里面的字符是zero_value或'其他字符',对于zero_value：置0  其他：置1
    if( len == 0 || data_stream == NULL || (offset + len > *(uint64_t*)bitmap && this->rexpand(offset + len)==merr ) ){
        return merr;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    for(uint64_t i=0;i<len;i++,offset++){
        if(data_stream[i] == zero_value){
            first[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
            continue;
        }
        first[offset/8] |= 1 << (offset%8); // 将该位置为1
    }
    return 0; // 成功设置，返回0
}

inline int BITMAP::set(uint64_t offset,uint8_t value){
    if(offset >= *(uint64_t*)this->bitmap && this->rexpand(offset+1)==merr ) {        
        return merr; //越界,自动扩容
    }
    if(value){//value=0 ：置0,  否则 ：置1
        (bitmap + sizeof(uint64_t))[offset/8] |= 1 << (offset%8); // 将该位置为1
        return 0;
    }
    (bitmap + sizeof(uint64_t))[offset/8] &= ~(1 << (offset%8)); // 将该位置为0
    return 0;
}

inline uint64_t BITMAP::size(){
    return *(uint64_t*)this->bitmap;
}

inline int BITMAP::iserr(){
    /*
    内存检查:
    1. bitmap是否为NULL
    2. bitmap的内存重申请
    状态检查:
    */
    if( this->bitmap==NULL || (this->bitmap = (uint8_t*)realloc(this->bitmap,(*(uint64_t*)this->bitmap + 7)/8 + sizeof(uint64_t))) == NULL ){
        return merr;
    }
    return 0;
}

inline uint64_t BITMAP::count(uint64_t st_offset,uint64_t ed_offset)//包括start和end,数1的个数
{
    if(bitmap == NULL || (st_offset > ed_offset || ed_offset >= *(uint64_t*)bitmap || st_offset >= *(uint64_t*)bitmap)){
        return merr;
    }
   
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    uint64_t sum = 0; // 初始化计数器
    for(;st_offset <= ed_offset;st_offset++){
        if(first[st_offset/8] & (1 << (st_offset%8)))sum++; // 如果该位为1，计数器加1
    }
    return sum; // 返回计数结果
}
// 打印位图的二进制表示
void BITMAP::ptf(){//测试用函数
    if (state != 0) { // 检查对象是否有效
        #ifdef bitmap_debug
        printf("BITMAP is in invalid state!\n");
        #endif
        return;
    }
    uint8_t* first = bitmap + sizeof(uint64_t); // 指向数据区
    for(uint64_t i=0;i<*(uint64_t*)bitmap;i++){
        printf("%d", first[i/8] & (1 << (i%8)) ? 1 : 0); 
    }
    printf("\n");
    return;
}

int64_t BITMAP::find(uint8_t value, uint64_t start, uint64_t end)
{
    if (start > end || end >= *(uint64_t*)this->bitmap) {
        return merr;
    }

    const uint64_t* p64 = (const uint64_t*)(this->bitmap + sizeof(uint64_t));
    uint64_t pos = start;

    // 1. 处理起始的未对齐部分和单块内的查找
    uint64_t first_word_idx = pos / 64;
    uint64_t end_word_idx = end / 64;
    
    uint64_t chunk = p64[first_word_idx];
    
    // 创建掩码，屏蔽掉 pos 之前的所有位
    uint64_t start_mask = ~0ULL << (pos % 64);
    
    // 如果搜索范围在同一个64位块内
    if (first_word_idx == end_word_idx) {
        // 创建掩码，屏蔽掉 end 之后的所有位
        uint64_t end_mask = ~0ULL >> (63 - (end % 64));
        uint64_t mask = start_mask & end_mask;
        chunk &= mask;
    } else {
        chunk &= start_mask;
    }

    if (value) { // 查找 1
        if (chunk != 0) {
            for (int i = pos % 64; i < 64; ++i) {
                if ((chunk >> i) & 1) {
                    if (first_word_idx * 64 + i <= end) {
                        return first_word_idx * 64 + i;
                    }
                }
            }
        }
    } else { // 查找 0
        uint64_t search_chunk = ~chunk & start_mask;
        if (first_word_idx == end_word_idx) {
            uint64_t end_mask = ~0ULL >> (63 - (end % 64));
            search_chunk &= end_mask;
        }
        if (search_chunk != 0) {
             for (int i = pos % 64; i < 64; ++i) {
                if ((search_chunk >> i) & 1) {
                     if (first_word_idx * 64 + i <= end) {
                        return first_word_idx * 64 + i;
                    }
                }
            }
        }
    }
    
    if (first_word_idx == end_word_idx) {
        return merr;
    }

    // 2. 处理中间的全量64位块
    for (uint64_t i = first_word_idx + 1; i < end_word_idx; ++i) {
        chunk = p64[i];
        if (value) { // 查找 1
            if (chunk != 0) {
                for (int j = 0; j < 64; ++j) {
                    if ((chunk >> j) & 1) return i * 64 + j;
                }
            }
        } else { // 查找 0
            if (chunk != ~0ULL) {
                for (int j = 0; j < 64; ++j) {
                    if (!((chunk >> j) & 1)) return i * 64 + j;
                }
            }
        }
    }

    // 3. 处理结尾的未对齐部分
    chunk = p64[end_word_idx];
    uint64_t end_mask = ~0ULL >> (63 - (end % 64));
    
    if (value) {
        chunk &= end_mask;
        if(chunk != 0) {
            for (int i = 0; i <= (end % 64); ++i) {
                if ((chunk >> i) & 1) return end_word_idx * 64 + i;
            }
        }
    } else {
        chunk = ~chunk & end_mask;
         if (chunk != 0) {
            for (int i = 0; i <= (end % 64); ++i) {
                if ((chunk >> i) & 1) return end_word_idx * 64 + i;
            }
        }
    }

    return merr; // 如果没有找到，则返回err
}
