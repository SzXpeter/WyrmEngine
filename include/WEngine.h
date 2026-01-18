//
// Created by pheen on 18/01/2026.
//
#pragma once

class WRenderer;
class WEngine
{
public:
    WEngine();

    void Run();

    void SetWindowSize(int width, int height) const;

private:
     WRenderer& renderer;

};