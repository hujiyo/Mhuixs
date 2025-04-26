#include <iostream>
#include <list>
#include <string>
#include <ctime>

int main() {
    std::string s1 = "hello12345678901234567890";
    std::string s2 = "world12345678901234567890";
    std::string s3 = "mhuixs12345678901234567890";
    std::string s4 = "list12345678901234567890";
    std::string s5 = "test12345678901234567890";

    clock_t start, ed;
    start = clock();

    for (int i = 0; i < 10000; i++) {
        std::list<std::string> list;
        list.push_front(s1);
        list.push_front(s2);
        list.push_front(s3);
        list.push_front(s4);
        list.push_front(s5);
        list.push_back(s1);
        list.push_back(s2);
        list.push_back(s3);
        list.push_back(s4);
        list.push_back(s5);

        for (int j = 0; j < 10; j++) {
            if (!list.empty()) {
                list.pop_front();
            }
        }
    }
    ed = clock();
    int tm = ed - start;
    std::cout << "time: " << tm << std::endl;system("pause");
	
    return 0;
}    