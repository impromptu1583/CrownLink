#include <iostream>
#include "ThQueue.h"

#include <thread>

int main(int argc, char** argv) {
    ThQueue<int> queue;
    std::jthread thread{[&queue]{
        try {
            while (true) {
                int i = queue.pop();
                std::cout << i << std::endl;
            }
        } catch (const ThQueueEnded&) {
            // end...
        }
    }};

    queue.push(123);
    queue.end();


    return 0;
}