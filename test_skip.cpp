#include <iostream>
using namespace std;
int main() {
    int minWidth = 1;
    long long skipped = 0;
    long long total = 0;
    int maxW = 2;
    for(int cx=-maxW; cx<=maxW; ++cx) {
        for(int cz=-maxW; cz<=maxW; ++cz) {
            total++;
            if (minWidth > 0 && 
                cx >= - minWidth + 1 && cx <=  minWidth - 1 &&
                cz >= - minWidth + 1 && cz <=  minWidth - 1) {
                skipped++;
            }
        }
    }
    cout << "Total: " << total << ", skipped: " << skipped << endl;
}
