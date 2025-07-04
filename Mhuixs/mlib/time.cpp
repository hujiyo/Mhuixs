#include <stdio.h>
#include <math.h>
#include "time.hpp"

bool Date::is_valid(int y, int m, int d) {// 辅助函数：检查是否为合法日期
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1) return false;
    int days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && (y%4 == 0 && y%100 != 0) || (y%400 == 0)) days_in_month[2] = 29;
    return d <= days_in_month[m];
}
Date::Date(){
    set(1970,1,1);
}
Date::Date(int date){
    if(is_valid(date/10000,(date/100)%100,date%100))this->date=date;
    else date=19700101;
}
Date::Date(int y, int m, int d) {
    set(y, m, d);
}
int Date::year()  const { return date / 10000; }
int Date::month() const { return (date / 100) % 100; }
int Date::day()   const { return date % 100; }
void Date::set(int y, int m, int d) {
    if (is_valid(y, m, d))date = y*10000 + m*100 + d;
    else date = 19700101;  // 非法日期设为默认值
}
int Date::to_days() const {
    int y = year(), m = month(), d = day();
    if (m < 3) { y--; m += 12; }
    return 365*y + y/4 - y/100 + y/400 + (153*(m-3)+2)/5 + d - 62;
}
void Date::add_days(int days) {
    int y = year(), m = month(), d = day();
    while (days-- > 0) {
        int dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
        if (m == 2 && (y%4 == 0 && y%100 != 0) || (y%400 == 0)) dim[2] = 29;
        if (++d > dim[m]) {
            d = 1;
            if (++m > 12) {
                m = 1;
                y++;
            }
        }
    }
    set(y, m, d);
}
int Date::days_between(const Date& b) {
    return abs(to_days() - b.to_days()); 
}

bool Time::is_valid(int h, int m, int s) {
    return (h >= 0 && h < 24) && 
            (m >= 0 && m < 60) && 
            (s >= 0 && s < 60);
}
Time::Time(){
    set(0,0,0);
}
Time::Time(int time){
    if(is_valid(time/10000,(time/100)%100,time%100))this->time=time;
    else time=0;
}
Time::Time(int h, int m, int s) { 
    set(h, m, s); 
}
int Time::hour()   const { return time / 10000; }
int Time::minute() const { return (time / 100) % 100; }
int Time::second() const { return time % 100; }
void Time::set(int h, int m, int s) {
    if (is_valid(h, m, s))time = h * 10000 + m * 100 + s;
    else time = 0;  // 非法时间重置为00:00:00
}
int Time::to_seconds() const {
    return hour() * 3600 + minute() * 60 + second();
}
void Time::add_seconds(int seconds) {
    int total = to_seconds() + seconds;
    total %= 86400;          // 处理超过一天
    if (total < 0) total += 86400;  // 处理负数
    set(total / 3600,        // 新小时
        (total % 3600) / 60, // 新分钟
        total % 60);        // 新秒
}
// 计算两个时间的秒数差
int Time::seconds_between(const Time& a, const Time& b) {
    return abs(a.to_seconds() - b.to_seconds());
}
