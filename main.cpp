///// for test
#include "PassManager.hpp"
#include <memory>


int main()
{
    auto PM = std::make_unique<_PassManager>();
    PM->RunImpl();
}