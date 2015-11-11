/*
 * This was useful for inserting small delays.
 */
#include <chrono>
#include <thread>

#include "delay.hh"
void delay (unsigned ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
