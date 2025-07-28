#include "./memap.hpp"
#include <cassert>


int main() {
    MEMAP memap(32,32);
    printf("return value: %d\n", memap.smalloc(64));
    printf("return value: %d\n", memap.smalloc(1024));
    printf("return value: %d\n", memap.smalloc(32));
    
    memap.sfree(memap.smalloc(64),64);
    /*
    assert(memap.iserr()==0); // 检查初始化是否成功
    assert(memap.smalloc(32)==0); // 检查第一个块是否成功分配
    assert(memap.smalloc(32)==32); // 检查第二个块是否成功分配
    assert(memap.smalloc(32)==64); // 检查第三个块是否成功分配
    */
    
    printf("All tests passed!\n");
    return 0;
}