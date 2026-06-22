#include <iostream>

namespace SparkHost {
    bool bReinit = false;
    bool bExit = false;
    void reinitialize() {
        std::cout << "Reinitializing..." << std::endl;
        bReinit = true;
    }
    void exit() {
        std::cout << "Exiting..." << std::endl;
        bExit = true;
    }
}
