#include "uintdeque.hpp"
#include <iostream>
#include <deque>
using namespace std;
#define MAX 1000000
/*
int main(){
    deque<int> dq;
    for(int i = 0; i < MAX/2; i++){
        dq.push_front(i); 
    }
    for(int i = MAX/2,j=0; i < MAX; i++,j++){
        dq.insert(dq.begin()+j,i); 
    }
    cout<<dq.size()<<endl;
    for(int i = 0; i < MAX-5; i++){
        dq.pop_back(); 
    }
    for(int i = 0; i < 5; i++){
        int a = dq.back();
        dq.pop_back();
        cout << a << endl; 
    }
}
*/
#define MAIN
#ifdef MAIN
int main() {
    UintDeque dq;
    
    for(int i = 0; i < MAX/2; i++){
        dq.lpush(i);
    }
    for(int i = MAX/2,j=0; i < MAX; i++,j++){
        dq.insert(j,i);
    }
    cout<<dq.size()<<endl;

    for(int i = 0; i < MAX-5; i++){
        dq.rpop();
    }
    for(int i = 0; i < 5; i++){
        int a = dq.rpop();
        cout << a << endl;
    }
}
#endif
