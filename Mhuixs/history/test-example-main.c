/*
#版权所有 (c) Mhuixs-team 2024
#许可证协议:
#任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
start from 2024.11
Email:hj18914255909@outlook.com
*/

#include "tblh.h"
#include <stdlib.h>

int main() {
	TABLE table;//创建一个TABLE对象
	Tblh tblh;//创建一个Tblh函数集
	initTblh(&tblh);//初始化Tblh函数集

	FIELD field[5];
	//初始化字段
	initFIELD(&field[0], "id", ui1);
	initFIELD(&field[1], "name",s50);
	initFIELD(&field[2], "age", ui1);
	initFIELD(&field[3], "sex", s50);
	initFIELD(&field[4], "address", s50);
	//创建表
	tblh.make_table(&table, "information", field,5);
    //增加记录
	tblh.add_record(&table,"1,huji,19,male,beijing");
	tblh.add_record(&table, "2,huji,19,male,beijing");

    //打印表
	tblh.printf_table(&table, 0);//在第0行开始打印
	

	system("pause");
}