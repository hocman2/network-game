#include "game.h"
#include <cstring>
#include <print>
#include "net.h"

using namespace std;

int main(int argc, char* argv[]) {
    
    if (argc > 1 && (strcmp(argv[1], "--host") || strcmp(argv[1], "-h"))) {
        println("Host mode");        
        start_host();
    } else {
        println("Client mode");
        start_client();
    }

    //run_game();
}
