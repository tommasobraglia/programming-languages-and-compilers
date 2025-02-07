#include <stdio.h>

#define N 9

void loopfusion(int a[N]){
    for (int i=0; i<N; i++)
        a[i]=i;
    for (int i=0; i<N; i++){
        int x = a[i]+2;
    }
}

int main() {
    int a[N]= {1,2,3,4,5,6,7,8,9};

    loopfusion(a);

    return 0;
}