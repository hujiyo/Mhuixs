#include "datstrc/bitmap/bitmap.h"
/*
huji (c) 2024.12.13  @SZTU
这是测试bitfh.h的测试程序
*/
int main(){
    BITMAP bitmap = getBITMAP(13);
    printBITMAP(bitmap);

    setBIT(bitmap,3,1);
    printBITMAP(bitmap);

    setBIT(bitmap,3,0);
    printBITMAP(bitmap);

    setBIT(bitmap,12,1);
    printBITMAP(bitmap);

    setBIT(bitmap,11,1);
    printBITMAP(bitmap);

    setBIT(bitmap,0,1);
    printBITMAP(bitmap);

    printf("0-12have %d bits\n",countBIT(bitmap,0,12));
}