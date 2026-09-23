#include "Console.h"

int main() {
    Console console;
    if (!console.init()) {
        return -1;
    }
    console.run();
    return 0;
}