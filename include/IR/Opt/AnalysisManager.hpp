#pragma once
#include "Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/Singleton.hpp"
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"


class AnalysisManager : public _AnalysisBase<AnalysisManager, Function>
{

public:
    void run();

    template<typename Pass>
    const auto& get()
    {
        using Result = typename Pass::Result;
        return nullptr;
    }

    DominantTree& getTree()
    {
        
    }

    AnalysisManager();
    ~AnalysisManager() = default;
};
