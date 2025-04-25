#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define merr -1
#define TENTATIVE 0
#define _max_(a,b) ((a)>(b)?(a):(b))

#define long_record      400  //定义了多大才算是一个长记录
#define short_record_s_vacancy_rate 1.5  //短记录的留余空间占比
#define long_record_s_vacancy_rate 1.2	//长记录的留余空间占比
#define begin_ROM 200		//初始记录的总空间ROM
#define add_ROM 100			//每次扩展记录空间
#define record_add_ROM 300		//每次扩展每条记录的空间
#define format_name_length 50		//字段名 占用字节数

#define separater ','		//定义字段分隔符

#define TBL_HEAD_SIZE 100  //文件头HEAD的大小最大不超过100字节

#define short_string 50
#define long_string 300
//数据类型位 占用1字节
#define I1	'a'		// -128~127
#define I2	'b'		// -32768~32767
#define I4	'c'		// -2147483648~2147483647
#define I8	'd'		// -9223372036854775808~9223372036854775807
#define UI1	'e'		// 0~255
#define UI2	'f'		// 0~65535
#define UI4	'g'		// 0~4294967295
#define UI8	'h'		// 0~18446744073709551615
#define F4	'i'		
#define F8	'j'	
#define STR 'k'		
#define DATE		'l'		//* 年* 月* 日 4byte 2 + 1 + 1
#define TIME		'm'		// * 时 * 分 * 秒 3byte 1 + 1 + 1

struct str{
    uint8_t *string;//STREAM:字节流的长度
    uint32_t len;//字节流的长度
    int state;//状态码
    str(char* s):len(strlen(s)),state(0),string((uint8_t*)malloc(strlen(s))){
		if(string == NULL){
			#ifdef bitmap_debug
			printf("str init malloc error\n");
			#endif
			len = 0;
			state++;
		}
		memcpy(string, s, len);
		return;
	}
	str(uint8_t *s, uint32_t len):len(len),state(0),string((uint8_t*)malloc(len)){
		if(string == NULL){
			#ifdef bitmap_debug
			printf("str init malloc error\n");
			#endif
			len = 0;
			state++;
			return;
		}
		memcpy(string, s, len);
		return;
	}
    str(str& s):len(s.len),state(s.state),string((uint8_t*)malloc(s.len)){
		if(string == NULL){
			#ifdef bitmap_debug
			printf("str init malloc error\n");
			#endif
			len = 0;
			state++;
			return;
		}
		memcpy(string, s.string, s.len);
		return;
	}
	~str(){
		free(string);
	}
};
struct Date {
    int date;// 存储格式：YYYYMMDD

    static bool is_valid(int y, int m, int d) {// 辅助函数：检查是否为合法日期
        if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1) return false;
        int days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (m == 2 && (y%4 == 0 && y%100 != 0) || (y%400 == 0)) days_in_month[2] = 29;
        return d <= days_in_month[m];
    }

    Date(int y=1970, int m=1, int d=1) { set(y, m, d); }
    int year()  const { return date / 10000; }
    int month() const { return (date / 100) % 100; }
    int day()   const { return date % 100; }
    void set(int y, int m, int d) {
        if (is_valid(y, m, d))date = y*10000 + m*100 + d;
        else date = 19700101;  // 非法日期设为默认值
    }
};
struct Time {
    int time;  //存储格式：HHMMSS
    // 辅助函数：检查时间合法性
    static bool is_valid(int h, int m, int s) {
        return (h >= 0 && h < 24) && 
               (m >= 0 && m < 60) && 
               (s >= 0 && s < 60);
    }
    
    // 构造函数（默认00:00:00）
    Time(int h=0, int m=0, int s=0) { set(h, m, s); }
    // 基本访问函数
    int hour()   const { return time / 10000; }
    int minute() const { return (time / 100) % 100; }
    int second() const { return time % 100; }
    // 设置时间（非法时间设为00:00:00）
    void set(int h, int m, int s) {
        if (is_valid(h, m, s))
            time = h * 10000 + m * 100 + s;
        else
            time = 0;  // 非法时间重置为00:00:00
    }
};

typedef struct LINE_INDEX {
	uint32_t sequence;//LINE_INDEX[j].sequence中j表示(虚)顺序，sequence表示(实)顺序
	uint8_t* rcd_addr;//由于record_length相同，直接储存每一行的首地址会更快
}L_INDEX, LINE_INDEX;

typedef struct IDLE_MAP {
	uint32_t idle_size;//空位的可用大小
	uint32_t idle_offset;//空位的record偏移量
}IDLE_MAP;//“记录使用区”的“空位内存地图”

typedef struct FIELD {
	char type;
	char name[format_name_length];//字段名 注意\0结尾
}FIELD;

class TABLE {
private:
    FIELD* p_field; //TABLE字段区地址（通过字段数确定边界！！！）//集成索引功能 删除FIELD_INDEX索引对象
    uint32_t field_num;//字段数
    uint32_t* offsetofield;//最新字段偏移量方案，p_p_field[i]表示第i个字段信息结构体，它的记录偏移量就是offsetofield[i],它将始终与对应的FIELD一一对应
    IDLE_MAP* idle_map;//“记录使用区”的“空位内存地图”
    uint32_t map_size;//空位的数量，这个数量实际应该不会特别大，除非你在天天删除字段但是却不增添字段，这显然不可能

    uint8_t* p_data;//TABLE数据区地
    uint32_t record_length;//记录长度（记录使用区大小+字段未使用区的大小）
    uint32_t record_usage;//记录长度（记录使用区大小：这里面包括字段删除留下的空位）
    uint32_t record_num;//记录条数（行数）	
    L_INDEX* line_index;//行索引

    uint32_t data_ROM;//TABLE数据区record条数总容量
    char* table_name;//table名

	int state;
public:
	TABLE(char* table_name, FIELD* field, uint32_t field_num);
	~TABLE();

	int64_t add_record(const char* record);
	int8_t rmv_record(uint32_t j);
	int8_t swap_record(uint32_t j1, uint32_t j2);
	uint8_t insert_record(const char* record, uint32_t j);

	uint32_t add_field(FIELD* field); 
	int8_t rmv_field(uint32_t i);
	int8_t swap_field(uint32_t i1_, uint32_t i2_);
	int8_t insert_field(FIELD* field, uint32_t i);

	int8_t getfrom_i_j(uint32_t i, uint32_t j, void* buffer);
	uint8_t reset_table_name(char* table_name);
	void print_table(uint32_t start_line);
	void print_record(uint32_t line_j,uint32_t start_line);
	//下面三个有bug，但是我找不到哪里有问题
	void save_table(char* file_path);
	int8_t load_table(char* file_path);
	int join_record(TABLE* table_join);
private:
	void flash_line_index(TABLE* table);
	uint8_t copymemfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer);
	uint8_t* address_of_i_j(TABLE *table,uint32_t i,uint32_t j);
};

static void cpstr(uint8_t* start, uint8_t* end, uint8_t* target){
	for (int i = 1; i < (end - start); *target = *(start + i), target++, i++);//复制start和end "中间" 的信息到target中
}
static void gotoxy(uint32_t x, uint32_t y){
	printf("\033[%d;%dH", ++y, ++x);//原点为（0，0）
}
static int sizeoftype(char type) { //返回某类型在p_data池中的占用的字节数	
	switch (type){
		case I1:case UI1:return 1;
		case I2:case UI2:return 2;
		case I4:case UI4:case F4:case STR:case DATE:case TIME:return 4;
		case I8:case UI8:case F8:return 8;
		default: return merr;
	}
}
static int ptfsizeoftype(char type){//根据数据类型返回打印时占用的字节数
	switch (type){
		case I1:return 4 + 1;//-128占用4个位数，加一表示至少1个空格
		case I2:return 6 + 1;//-32768占用6个位数
		case I4:return 11 + 1;//-2147483648占用11个位数
		case I8:return 20 + 1;//-9223372036854775808占用20个位数
		case UI1:return 3 + 1;//255占用3个位数
		case UI2:return 5 + 1;//65535占用5个位数
		case UI4:return 10 + 1;//4294967295占用10个位数
		case UI8:return 20 + 1;//18446744073709551615占用20个位数
		case F4:return 7 + 1;//总位数不超过7位
		case F8:return 15 + 1;//总位数不超过15位
		case STR:return 10 + 1;//打印前10个字符
		case DATE:return 10 + 1;//2024.10.10
		case TIME:return 8 + 1;//11:11:11
		default: return merr;
	}
}

void TABLE::flash_line_index(TABLE* table){
	/*
	当p_data或record_length改变
	调用此函数以更新line_index的快速指针
	*/
	//缓存数据
	uint8_t* new_p_data = table->p_data;
	uint32_t record_length = table->record_length;
	uint32_t record_num = table->record_num-1;
	L_INDEX* line_index = table->line_index;

	for(;record_num>0;
		line_index[record_num].rcd_addr = new_p_data + record_length * record_num,
		record_num--);

	line_index[0].rcd_addr = new_p_data;//第一行的地址
}
static uint8_t store_fieldata(char* p_inputstr, uint8_t* p_storaddr, char type){
	/*
	tblh_add_record函数的底层储存函数
	把字符串类型p_inputdata按照type类型进行自动储存
	使用put_tobyte处理C语言标准数据的储存
	p_inputdata必须以\0结尾
	put_tobyte存储C语言标准数据类型
	storage_field_data存储SQlh标准数据类型
	*/
	switch (type){
		case I1:
			*p_storaddr = atoi((const char*)p_inputstr);//这里可以考虑使用更快速的函数atoi
			return 0;
		case I2: {
			int16_t i = atoi((const char*)p_inputstr);//在后期调试时，这里可以尝试改成atoi
			memcpy(p_storaddr,&i,sizeoftype(I2));
			return 0;
		}
		case I4:case DATE:case TIME: {
			int32_t i = atoi((const char*)p_inputstr);
			memcpy(p_storaddr,&i,sizeoftype(type));
			return 0;
		}		   
		case I8: {
			int64_t i = strtoll((const char*)p_inputstr, NULL, 10);//长字节使用strtoll
			memcpy(p_storaddr,&i,sizeoftype(I8));
			return 0;
		}			
		case UI1:
			*p_storaddr = atoi((const char*)p_inputstr);
			return 0;
		case UI2: {
			uint16_t i = atoi((const char*)p_inputstr);
			memcpy(p_storaddr,&i,sizeoftype(UI2));
			return 0;
		}				
		case UI4: {
			uint32_t i = strtoul((const char*)p_inputstr, NULL, 10);
			memcpy(p_storaddr,&i,sizeoftype(UI4));
			return 0;
		}				
		case UI8: {
			uint64_t i = strtoull((const char*)p_inputstr, NULL, 10);
			memcpy(p_storaddr,&i,sizeoftype(UI8));
			return 0;
		}				
		case F4: {
			float i = strtof((const char*)p_inputstr, NULL);
			memcpy(p_storaddr,&i,sizeoftype(F4));
			return 0;
		}			
		case F8: {
			double i = strtod((const char*)p_inputstr, NULL);
			memcpy(p_storaddr,&i,sizeoftype(F8));
			return 0;
		}
		case STR:{
			str* s = (str*)malloc(sizeof(str));//创建一个str用来存储数据
			s->len=strlen(p_inputstr);
			s->string=(uint8_t*)malloc(s->len);
			memcpy(s->string,p_inputstr,s->len);
			memcpy(p_storaddr,s,sizeoftype(STR));
			return 0;
		}
	}
	return 1;
}
TABLE::TABLE(char* table_name, FIELD* field, uint32_t field_num):
p_field(NULL),field_num(field_num),
offsetofield(NULL),idle_map(NULL),
map_size(0),/*map_size是真实大小，不是数组中括号中的最大值*/p_data(NULL),
record_length(TENTATIVE),record_usage(TENTATIVE),
record_num(0),line_index(NULL),
data_ROM(begin_ROM),/*/TABLE数据区record条数初始容量为200条数*/table_name(NULL),
state(0){
	//初始化table基础信息
	uint32_t record_usage = 0;
	for (uint32_t i = 0; i < field_num; record_usage += sizeoftype(field[i].type), i++);
	this->record_usage = record_usage;
	this->record_length = 
	record_usage * (	//根据现有字段的占用长度合理的选择留白率
		(record_usage < long_record) ? short_record_s_vacancy_rate : long_record_s_vacancy_rate
	);

	// 创建p_head区,并创建对应的偏移量索引
	this->p_field = (FIELD*)calloc(field_num , sizeof(FIELD));//字段区域
	// 下面这行存在错误：sizeof(uint32_t*)应改为sizeof(uint32_t)
	// 原始代码：
	// this->offsetofield = (uint32_t*)malloc(field_num * sizeof(uint32_t*));//字段偏移量索引
	// 修正后：
	this->offsetofield = (uint32_t*)malloc(field_num * sizeof(uint32_t));//字段偏移量索引
	this->idle_map = (IDLE_MAP*)malloc(sizeof(IDLE_MAP));//记录使用区的空位内存地图
	this->p_data = (uint8_t*)calloc(1,this->record_length * begin_ROM);//创建TABLE数据区
	this->line_index = (L_INDEX*)malloc(sizeof(L_INDEX));//先建立一个line_index	
	this->table_name = (char*)calloc(1,format_name_length);//创建table名区
	if(!this->p_field+!this->offsetofield+!this->idle_map+!this->p_data+!this->line_index+!this->table_name){
		free(this->p_field);
		free(this->offsetofield);
		free(this->idle_map);
		free(this->p_data);
		free(this->line_index);
		free(this->table_name);

		#ifdef tblh_debug
		printf("TABLE init err:calloc error\n");
		#endif
		state++;
		return;
	}

	//将字段的偏移量存入offsetofield数组中
	uint32_t ofsum = 0;	
	for (uint32_t i = 0; i < field_num; 
		this->p_field[i] = field[i],
		this->offsetofield[i] = ofsum, 
		ofsum += sizeoftype(field[i].type), 
		i++
	);

	reset_table_name(table_name);
	return;
}
int64_t TABLE::add_record(const char* record)//add总是在末尾追加
{
	//如果record为空，则增加一条空记录
	int isnullrecord = 0;
	if (record == NULL){
		isnullrecord = 1;
	}

	/*
	tblh_add_record把新记录加到p_data的末尾、line_index的索引组织，未来可能还要处理分页管理
	函数返回 虚序列号

	record的格式："字段1,字段2,字段3..."
	*/

	if (this->record_num == this->data_ROM) 	{
		//记录数容量满了:扩展
		//不用担心有空位没有利用，记录条数永远等于实际记录条数
		this->p_data = (uint8_t*)realloc(this->p_data, this->record_length * (this->data_ROM + add_ROM));
		if (this->p_data == NULL){
			return merr;
		}
		//更新p_data后要注意要刷新line_index#######!!!!!!!!!!!!!!!!!!!!!!###############################
		flash_line_index(this);
		this->data_ROM += add_ROM;
	}

	/*
	定位本条待写入记录的首地址即p_data的末尾    
	（不用担心内存中存在空位 ：tblh_rmv_record将会把最后一个数据填补到删除的记录处）
	*/
	uint8_t* new_record_address = this->p_data + this->record_length * this->record_num;//更新record_num后定位地址
	this->record_num++;//记录数更新

	//重新创建line_index索引
	this->line_index = (L_INDEX*)realloc(this->line_index, this->record_num * sizeof(L_INDEX));
	if (this->line_index == NULL){
		free(this->p_data);
		return merr;
	}
	this->line_index[this->record_num - 1].sequence = this->record_num - 1;//虚顺序也是表格的最后一个//序号都是从0开始，所以这里-1
	this->line_index[this->record_num - 1].rcd_addr = new_record_address;//记录记录首地址
	//写入记录
	memset(new_record_address, 0, this->record_length);//注意！写入操作前先归0！！！！
	if (isnullrecord){
		return this->record_num - 1;//如果是空记录，则不写入数据	
	}
	char* pointer = (char*)record;//初始化 输入字符串 的定位指针
	uint32_t field_num = this->field_num;//初始化 字段数量
	for (uint32_t i = 0; i < field_num; i++)
	{
		if (i){//如果不是第一个字段，则每次定位到,后都进一位
			pointer = strchr(pointer, separater) + 1;
		}
		if (pointer == (char*)1){
			return 0;//如果已经到record的结尾'\0'
		}

		char result[long_string];//足够的大小来储存(这里可能会不够)<----扯淡
		memset(result, 0, long_string);//保证以\0结尾

		if (i != field_num - 1){
			cpstr((uint8_t*)(pointer - 1), (uint8_t*)strchr(pointer, separater), (uint8_t*)result);//获得本字段对应的字符串
		}
		else{
			cpstr((uint8_t*)(pointer - 1), (uint8_t*)strchr(pointer, '\0'), (uint8_t*)result);
		}
		//store_fieldata自动将目标字符串按照type的种类存入p_data中
		store_fieldata(result,new_record_address + this->offsetofield[i], this->p_field[i].type);
	}

	return this->record_num - 1;
}
int8_t TABLE::rmv_record(uint32_t j)//删除虚序号及其对应的record
{
	/*
	本函数实际上处理的是末记录和删除记录的关系
	用最后一条实记录去补删除的记录留下的空位
	保证记录的连续性（记录条数永远等于实际记录条数）
	*/

	//判断j记录是否存在
	if (j >= this->record_num){
		return merr;
	}
	memset(this->line_index[j].rcd_addr, 0, this->record_length);//清空j记录

	//判断本j记录是否是末记录（实记录）
	if (this->line_index[j].sequence != this->record_num - 1) 
	{
		//不是末记录就去找到sequence为record_num的记录（末记录）
		uint32_t j_mo = 0;
		for (; j_mo < this->record_num; j_mo++) 
		{
			if (this->line_index[j_mo].sequence == this->record_num - 1){
				break;//找到末记录对应的虚序列
			}
		}

		//把末记录复制到j处
		cpstr(this->line_index[j_mo].rcd_addr - 1, 
		this->line_index[j_mo].rcd_addr + this->record_length, 
		this->line_index[j].rcd_addr
		);
		memset(this->line_index[j_mo].rcd_addr, 0, this->record_length);//原来的末记录归0//想了很久还是增加这一步吧，省点力气
		
		//修改末记录对应的索引指向j地址
		this->line_index[j_mo] = this->line_index[j];
	}
	for (uint32_t i = j; i < this->record_num - 1; this->line_index[i] = this->line_index[i + 1], i++);//索引进位
	//缩小索引空间
	this->line_index = (L_INDEX*)realloc(this->line_index, sizeof(L_INDEX) * (--this->record_num));

	return 0;
}
int8_t TABLE::swap_record(uint32_t j1, uint32_t j2)//函数用于交换两条记录的排序 返回0表示成功
{
	//tblh的实现方法是虚序列号对应的索引的交换
	if (j1 >= this->record_num || j2 >= this->record_num){
		return merr;//验证j1、j2是否存（j1，j2一定是正数）
	}
	//交换j1和j2的索引
	L_INDEX cache = this->line_index[j1];
	this->line_index[j1] = this->line_index[j2];
	this->line_index[j2] = cache;

	return 0;
}
uint8_t TABLE::insert_record(const char* record, uint32_t j)//向j处插入记录，原来及后面的记录全部退一位
{
	uint32_t sep = this->add_record(record);//这个地方调用了外部函数，不知道会不会影响性能
	if (j >= sep){
		return 0;//已经增加了一条记录，如果现在j比最后一条记录都大或等，那就直接return了
	}
	L_INDEX cache = this->line_index[sep];//先调用tblh_add_record并保存其索引
	for (uint32_t k = sep; k > j; this->line_index[k] = this->line_index[k - 1], k--);
	this->line_index[j] = cache;

	return 0;//执行完返回0
}
int8_t TABLE::rmv_field(uint32_t i)
{
	//判断i是否合法
	if (i >= this->field_num){
		return merr;
	}
	/*
	由于字段的长度不一，再者对整列数据进行移动填补效率实在太低了
	所以空位就留下来，它所造成的内存浪费怎么解决呢？由以下机制来弥补：
	1.TABLE对象增加一个“记录使用区”的“空位内存地图”（idle_map）（注意：使用记录使用区的内存并不会改变record_usage）当下次增加字段的时候会首先查询idle_map中是否有适合大小的空位
	  只要新插入数据小于等于空位，就会占用空位，使空位变小或消失
	2.使用tblh_del_idle(...)整理数据库字段，这个函数将删除所有空位并整理字段信息
	3.如果是删除最后一个字段，则不会增加map，而是直接修改record_usage
	*/

	//首先判断删除的是否是最后一个字段
	if (i != this->field_num - 1) 
	{
		//增加this中的idle_map的大小
		this->idle_map = (IDLE_MAP*)realloc(this->idle_map, ++this->map_size * sizeof(IDLE_MAP));//map_size++;
		if(this->idle_map == NULL){
			return merr;
		}
		this->idle_map[this->map_size - 1].idle_size = sizeoftype(this->p_field[i].type);//获得删除字段的大小
		this->idle_map[this->map_size - 1].idle_offset = this->offsetofield[i];//得到空闲位置的偏移量
	}
	else 	{
		this->record_usage -= sizeoftype(this->p_field[i].type);//直接修改record_usage
	}

	//清空删除字段对应的数据区中的数据	
	//这样可以保证record_usage内部的空位绝对是归0的，对于record_length-record_usage中的区域，我称之为分配的到的未定义区域

	//先缓存数据cache
	uint32_t cc_table_record_length = this->record_length;
	uint8_t* cc_table_p_a_i = this->p_data + this->offsetofield[i];
	uint32_t cc_clean_size = this->idle_map[this->map_size - 1].idle_size;
	for (uint32_t j = 0; j < this->record_num; j++) {
		memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
	}
	if (i != this->field_num - 1) 
	{
		//对field和offsetof_field中删除的字段后的字段进行进位
		for (uint32_t ii = i; ii < this->field_num - 1; 
		this->p_field[ii] = this->p_field[ii + 1], 
		this->offsetofield[ii] = this->offsetofield[ii + 1], 
		ii++
		);//索引进位

		this->field_num--;//字段数-1
		this->p_field = (FIELD*)realloc(this->p_field, this->field_num * sizeof(FIELD));//字段数-1		

		this->offsetofield = (uint32_t*)realloc(this->offsetofield, this->field_num * sizeof(uint32_t));
	}
	else {
		this->field_num--;//字段数-1
		this->p_field = (FIELD*)realloc(this->p_field, this->field_num * sizeof(FIELD));
	}
	return 0;
}
uint32_t TABLE::add_field(FIELD* field){
	/*
	tblh_add_field先尝试遍历一遍idle_map,如果存在一个足够大小的空位，就把它写入空位，并重新修改idle_map
	把新字段对应的数据区映射到记录的末尾、field_index的索引组织
	函数返回 虚序列号
	*/
	this->field_num++;
	this->p_field = (FIELD*)realloc(this->p_field, this->field_num * sizeof(FIELD));
	if(this->p_field == NULL){
		return merr;
	}
	this->p_field[this->field_num - 1] = field[0];//加星号，field是一个结构体变量名(field[0]<=>*field)
	this->offsetofield = (uint32_t*)realloc(this->offsetofield, this->field_num * sizeof(uint32_t));

	//接下在p_data中为这个字段找合适的储存位置，先在idle_map里找
	uint32_t new_field_size = sizeoftype(field->type);
	uint32_t new_offset = this->record_usage;//默认偏移量
	uint32_t cc_map_size = this->map_size;
	IDLE_MAP* cc_idle_map = this->idle_map;

	//循环遍历找到合适的空位算法
	uint32_t idle_seq = 0;//记录之后填入的空位的序号
	for (uint32_t k = 0; k < cc_map_size; k++) {
		if (cc_idle_map[k].idle_size == new_field_size) 
		{
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;//记录填入的空位序号
			break;//如果有相等的空位，那不用多想，就这个了
		}
		if (cc_idle_map[k].idle_size > new_field_size && new_offset > cc_idle_map[k].idle_offset) 
		{
			new_offset = cc_idle_map[k].idle_offset;
			idle_seq = k;
		}//如果有够装的空间，则以偏移量小的优先
	}
	this->offsetofield[this->field_num - 1] = new_offset;//得到偏移量了
	//接下来判断new_offset是否为record_usage外，如果是，还要判断record_length够不够
	if (new_offset == this->record_usage) 
	{
		//如果不够，需要增加分配p_data的空间
		if (this->record_length - this->record_usage <= new_field_size) 
		{
			//如果剩余空间不够，增加分配p_data的空间
			uint32_t old_record_length = this->record_length;//缓存旧的record_length
			//计算新的record_length
			this->record_length += record_add_ROM;
			//重新分配p_data，注意：修改了p_data,字段索引的偏移量并不会改变，但是line_index的索引需要重新分配
			this->p_data = (uint8_t*)realloc(this->p_data, this->record_length * this->data_ROM);
			//接下来开始进行所有数据的位移
			if (this->record_num == 0){
				goto l;//uint32_t-1的情况
			}
			for (uint32_t k = this->record_num - 1; k > 0; k--) {
				//从最后一个记录开始,每条记录向后移动record_add_ROM*k,注意record_usage之后的空间都是空闲的(此时this->record_usage还没有进行修改)
				//首先要保证record_usage之中的空间是绝对统一的，然后才是对新字段空间的分配以及后续的归0
				memmove(this->p_data + this->record_length * k, this->p_data + old_record_length * k, this->record_usage);
				//只要执行到k=1,因为第一条记录不需要移动
			}
		l:
		flash_line_index(this);//刷新line_index，至此，扩容完毕		
		}
		this->record_usage += new_field_size;
		//清空新字段对应的数据区中的数据
		//使用空间record_usage已经增加,接下来进行归0操作,抄一下上面的代码。
		uint32_t cc_table_record_length = this->record_length;
		uint8_t* cc_table_p_a_i = this->p_data + this->offsetofield[this->field_num - 1];
		uint32_t cc_clean_size = new_field_size;
		for (uint32_t j = 0; j < this->record_num; j++) {
			memset(cc_table_p_a_i + cc_table_record_length * j, 0, cc_clean_size);
		}

		return this->field_num - 1;//返回虚序列号，序列号默认是最后一个
	}
	else {
		//如果new_offset在record_usage内部，则涉及idle_map的功能了
		//之前的所有代码已经保证record_usage内部的所有空位都已经清空了，这里不需要再清空一遍
		this->idle_map[idle_seq].idle_size -= new_field_size;
		if (this->idle_map[idle_seq].idle_size == 0) 
		{
			//如果这个空位正好被占满了，那么就把idle_map[idle_seq]删掉（用最后一个idle_map替换，无需担心如果只有一个（自己换自己））
			this->idle_map[idle_seq] = this->idle_map[--this->map_size];//天才！！之前怎么没想到替换
			this->idle_map = (IDLE_MAP*)realloc(this->idle_map, this->map_size * sizeof(IDLE_MAP));
			this->map_size--;
		}
		else {
			//如果idle_map还有空位，那么修改offset就好了
			this->idle_map[idle_seq].idle_offset += new_field_size;
		}
		return this->field_num - 1;
	}
}
int8_t TABLE::swap_field(uint32_t i1_, uint32_t i2_)
{
	/*
	tblh的独特设计使得字段交换成为非常简单的操作
	*/
	if (i1_ >= this->field_num || i2_ >= this->field_num){
		return merr;//验证11、12是否存
	}
	//交换i1和i2的索引
	FIELD cache = this->p_field[i1_];
	this->p_field[i1_] = this->p_field[i2_];
	this->p_field[i2_] = cache;
	uint32_t cache_offset = this->offsetofield[i1_];
	this->offsetofield[i1_] = this->offsetofield[i2_];
	this->offsetofield[i2_] = cache_offset;

	return 0;
}
int8_t TABLE::insert_field(FIELD* field, uint32_t i)
{
	uint32_t sep = this->add_field(field);
	if (i >= sep){
		return merr;//如果非法插入还是默认在最后插入
	}
	FIELD cache = this->p_field[sep];
	uint32_t cache_offset = this->offsetofield[sep];
	FIELD* cc_p_head = this->p_field;
	uint32_t* cc_offsetof_field = this->offsetofield;
	for (uint32_t k = sep; k > i; cc_p_head[k] = cc_p_head[k - 1], cc_offsetof_field[k] = cc_offsetof_field[k - 1], k--);
	this->p_field[i] = cache;
	this->offsetofield[i] = cache_offset;

	return 0;
}
int8_t TABLE::getfrom_i_j(uint32_t i, uint32_t j, void* buffer)
{
	/*
	//使用这个函数前你就应当知道你读取的数据到底是什么类型,并保证buffer缓冲区足够容纳数据
	从（i，j）虚拟位置获得数据
	本函数会根据数据类型自动返回相应的格式化数据
	*/
	//先获得第i个字段的类型、偏移量的信息
	char type = this->p_field[i].type;
	//找到（i,j）的地址
	uint8_t* inf_addr = this->p_data + j * this->record_length + this->offsetofield[i];
	//根据type类型进行格式化输出
	switch (type){
		case I1:case I2:case I4:case I8:			
		case UI1:case UI2:case UI4:case UI8:			
		case F4:case F8:case DATE:case TIME:
			memcpy(buffer, inf_addr, sizeoftype(type)); 
			return 0;
		case STR:
			str* p=(str*)inf_addr;
			memcpy(buffer,p->string,p->len); 
			return 0;
	}
	return merr;
}
uint8_t TABLE::copymemfrom_i_j(TABLE* table, uint32_t i, uint32_t j, void* buffer)
{
	/*
	从（i，j）虚拟位置获得数据
	内置函数，和tblh_getfrom_i_j相比，本函数直接在TABLE所管辖的数据内存区域直接复制到buffer中
	本函数不会根据数据类型自动返回相应的格式化数据
	*/
	//先获得第i个字段的类型、偏移量的信息
	char type = table->p_field[i].type;
	//找到（i,j）的地址
	uint8_t* inf_addr = table->p_data + j * table->record_length + table->offsetofield[i];
	memcpy(buffer,inf_addr,sizeoftype(type));

	return 0;
}
uint8_t* TABLE::address_of_i_j(TABLE *table,uint32_t i,uint32_t j){
	return table->p_data + j * table->record_length + table->offsetofield[i];
}
void TABLE::print_record(uint32_t j, uint32_t y)
{
	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * this->field_num);
	uint32_t address = 0;
	for (uint32_t i = 0; i < this->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(this->p_field[i].type), strlen(this->p_field[i].name) + 1);
	}
	for (uint32_t i = 0; i < this->field_num; i++) {
			gotoxy(ptfmap[i], y);//定位到打印位置			
			uint8_t* inf_addr = this->line_index[j].rcd_addr+this->offsetofield[i];//确定数据位置
			switch (this->p_field[i].type) {
				case I1:
					printf("%d", *(int8_t*)inf_addr); 
					break;
				case I2: {
					int16_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(I2));
					printf("%d", tp); 
					break;
				}
				case I4: {
					int32_t tp;
					memcpy(&tp, inf_addr, sizeoftype(I4));
					printf("%d", tp); 
					break;
				}
				case I8: {
					int64_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(I8));
					printf("%lld" /*PRIx64*/, tp); 
					break;//"lld"报错，不知道为什么
				}
				case UI1:
					printf("%d", *(uint8_t*)inf_addr); 
					break;
				case UI2: {
					uint16_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(UI2));
					printf("%d", tp); 
					break;
				}
				case UI4: {
					uint32_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(UI4));
					printf("%d", tp); 
					break;
				}
				case UI8: {
					uint64_t tp; 
					memcpy(&tp, inf_addr, sizeoftype(UI8));
					printf("%llu"/* PRIu64*/, tp); 
					break;
				}
				case F4: {
					float tp; 
					memcpy(&tp, inf_addr, sizeoftype(F4));
					printf("%.7g", tp); 
					break;
				}
				case F8: {
					double tp; 
					memcpy(&tp, inf_addr, sizeoftype(F8));
					printf("%.15g", tp); 
					break;
				}
				case STR:
					void* str_p;
					memcpy(str_p,inf_addr,sizeoftype(STR));
					printf("%.10s",((str*)str_p)->string); 
					break;
				case DATE: {
					Date tp;
					memcpy(&tp, inf_addr, sizeoftype(I4));
					printf("%d.%d.%d", tp.year(), tp.month(), tp.day()); 
					break;
				}
				case TIME: {
					Time tp; 
					getfrom_i_j(i, j, &tp);
					printf("%d:%d:%d", tp.hour(), tp.minute(), tp.second()); 
					break;
				}
			}
		}
	free(ptfmap);
}
void TABLE::print_table(uint32_t start_line)
{
	uint32_t y = start_line;

	//初始化ptfmap得到打印地图
	uint32_t* ptfmap = (uint32_t*)malloc(sizeof(uint32_t) * this->field_num);
	if(ptfmap==NULL){
		printf("Err:failed to malloc ptfmap\n");
		return;
	}
	uint32_t address = 0;
	for (uint32_t i = 0; i < this->field_num; i++) {
		ptfmap[i] = address;
		address += _max_(ptfsizeoftype(this->p_field[i].type), strlen(this->p_field[i].name) + 1);
	}

	//打印字段名
	for (uint32_t i = 0; i < this->field_num; i++) {
		gotoxy(ptfmap[i], y);
		printf("%s", this->p_field[i].name);
	}
	y++;

	//打印数据
	for (uint32_t j = 0; j < this->record_num; y++, j++) {
		this->print_record(j, y);
	}
	free(ptfmap);
}
uint8_t TABLE::reset_table_name(char* table_name)
{
	memset(this->table_name, 0, format_name_length);//先归0
	memcpy(this->table_name, table_name, strlen(table_name)%format_name_length);
	return 0;
}
TABLE::~TABLE()
{
	//删除表，此时表必须make或者load之后才能使用
	this->field_num=0,this->map_size=0,this->record_length=0,this->record_usage=0,
	this->record_num=0,this->data_ROM=0,this->state=0;
	free(this->p_field);
	free(this->p_data);
	free(this->line_index);
	free(this->offsetofield);
	free(this->idle_map);
	free(this->table_name);
	this->p_field=NULL,this->p_data=NULL,this->line_index=NULL,
	this->offsetofield=NULL,this->idle_map=NULL,this->table_name=NULL;
	return;
}



void initFIELD(FIELD* field, char* field_name, char type)
{
	field->type = type;
	memset(field->name, 0, format_name_length);//先归0
	memcpy(field->name, field_name, strlen(field_name) % format_name_length);
}

int main() {
    // 定义字段
    FIELD fields[3];
    initFIELD(&fields[0], "id", I4);       // 整数字段
    initFIELD(&fields[1], "name", STR);   // 字符串字段
    initFIELD(&fields[2], "salary", F4);  // 浮点数字段

    TABLE table("mytable", fields, 3); // 创建表，字段数量为3

	FIELD field[2];
	initFIELD(&field[0], "age", I4);       // 整数字段
    initFIELD(&field[1], "address", STR);   // 字符串字段

	
	//table.add_field(&field[0]);
	//table.add_field(&field[1]);
	//table.print_table(0);

    table.add_record("1,John Doe,50000,30,china"); // 添加记录
    table.add_record("2,Jane Smith,60000.00,30,china"); // 添加记录
    table.add_record("3,Michael Johnson,70000.00,40,china"); // 添加记录
    table.print_table(0); // 打印表

	//table.swap_field(2,4);//交换salary和address
	//table.swap_record(1,2);

	//table.print_table(7); // 打印表

    return 0;
}