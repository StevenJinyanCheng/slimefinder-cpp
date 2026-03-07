#include <iostream>
using namespace std;
int main() {
    int startZ = -12752;
    int startInZ = startZ & 15;
    cout << startInZ << endl;
    cout << (-12753 & 15) << endl;
}
