#include <deque>
#include <iostream>
using namespace std;
/*
int main() {
    deque<int> dq;
    for(int i = 0; i < 1000000; i++) {
        dq.push_back(i);
        dq.push_front(i);
        dq.insert(dq.begin() + i/2, i);
    }
    printf("end\n");

}*/

#include <list>
int main() {
    list<int> dq;
    for(int i = 0; i < 1000000; i++) {
        dq.push_back(i);
        dq.push_front(i);
        auto it = dq.begin();
        for(int j = 0; j < i/2; j++) {
            it++;
        }
        dq.insert(it, i);
    }
    printf("end\n");
}