#pragma once

class BackendPassBase
{
public:
    virtual bool run()=0;
    BackendPassBase() = default;
    ~BackendPassBase() = default;
};