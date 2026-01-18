//
// Created by pheen on 18/01/2026.
//

#include "WEngine.h"

#include <WRenderer.h>

WEngine::WEngine() : renderer(WRenderer::GetInstance())
{}

void WEngine::Run()
{
    renderer.InitWindow();
    renderer.InitVulkan();

    while (!glfwWindowShouldClose(renderer.GetWindow()))
    {
        glfwPollEvents();
        renderer.DrawFrame();
    }

    renderer.Cleanup();
}

void WEngine::SetWindowSize(const int width, const int height) const
{
    renderer.SetWindowSize(width, height);
}
