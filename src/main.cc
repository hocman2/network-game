#include "game.h"
#include <cstring>
#include <print>
#include <thread>
#include "net.h"

using namespace std;

int main(int argc, char* argv[]) {
    
    thread net_thread;

    if (argc > 1 && (strcmp(argv[1], "--host") || strcmp(argv[1], "-h"))) {
        println("Host mode");       
        net_thread = thread(start_host); 
    } else {
        println("Client mode");
        net_thread = thread(start_client); 
    }

    run_game();

    // need to gracefully shutdown net thread here
}
