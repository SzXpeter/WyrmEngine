//
// Created by pheen on 02/01/2026.
//

#include "WEngine.h"

#include <iostream>

int main()
{
    WEngine engine;
    engine.SetWindowSize(1920, 1080);

    try
    {
        engine.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}