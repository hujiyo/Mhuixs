#include <stdio.h>
#include <math.h>

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
    int to_days() const {
        int y = year(), m = month(), d = day();
        if (m < 3) { y--; m += 12; }
        return 365*y + y/4 - y/100 + y/400 + (153*(m-3)+2)/5 + d - 62;
    }
    void add_days(int days) {
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
    int days_between(const Date& b) {
        return abs(to_days() - b.to_days()); 
    }
};

struct Time {
    int time;  //存储格式：HHMMSS
    static bool is_valid(int h, int m, int s) {
        return (h >= 0 && h < 24) && 
               (m >= 0 && m < 60) && 
               (s >= 0 && s < 60);
    }
    Time(int h=0, int m=0, int s=0) { set(h, m, s); }
    int hour()   const { return time / 10000; }
    int minute() const { return (time / 100) % 100; }
    int second() const { return time % 100; }
    void set(int h, int m, int s) {
        if (is_valid(h, m, s))
            time = h * 10000 + m * 100 + s;
        else
            time = 0;  // 非法时间重置为00:00:00
    }
    int to_seconds() const {
        return hour() * 3600 + minute() * 60 + second();
    }
    void add_seconds(int seconds) {
        int total = to_seconds() + seconds;
        total %= 86400;          // 处理超过一天
        if (total < 0) total += 86400;  // 处理负数
        set(total / 3600,        // 新小时
           (total % 3600) / 60, // 新分钟
            total % 60);        // 新秒
    }
    // 计算两个时间的秒数差
    static int seconds_between(const Time& a, const Time& b) {
        return abs(a.to_seconds() - b.to_seconds());
    }
};