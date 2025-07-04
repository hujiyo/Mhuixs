#ifndef TIME_HPP
#define TIME_HPP
#include <stdio.h>
#include <math.h>

struct Date {
    int date; // 存储格式：YYYYMMDD
    static bool is_valid(int y, int m, int d); // 辅助函数：检查是否为合法日期
    Date();
    Date(int);
    Date(int y, int m, int d);
    // 基本函数
    int year() const;
    int month() const;
    int day() const;
    void set(int y, int m, int d);//设置日期
    
    int to_days() const;// 返回从基准日（0000-01-01）开始的天数
    void add_days(int days);// 添加指定天数
    int days_between(const Date& b);// 计算两个日期的天数差
};

struct Time {
    int time;  // 存储格式：HHMMSS
    static bool is_valid(int h, int m, int s);// 辅助函数：检查时间合法性
    Time();
    Time(int);
    Time(int h, int m, int s);
    // 基本函数
    int hour() const;
    int minute() const;
    int second() const;
    void set(int h, int m, int s);// 设置时间（非法时间设为00:00:00）

    int to_seconds() const;// 转换为当天总秒数
    void add_seconds(int seconds);// 添加秒数（支持负数
    static int seconds_between(const Time& a, const Time& b);// 计算两个时间的秒数差
};

#endif  