#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <assert.h>
#include <dirent.h>
#include <math.h>
#include <queue>
#include <stack>
#include <map>
#include <set>

class Context;
struct Exception {};
enum {
    EAV = 0, AVE = 1, VEA = 2,
    EVA = 3, AEV = 4, VAE = 5
};
typedef uint64_t ArchitectureType;
typedef ArchitectureType Symbol;
union Triple {
    Symbol pos[3];
    struct {
        Symbol entity, attribute, value;
    };
    Triple() {};
    Triple(Symbol _entity, Symbol _attribute, Symbol _value)
        :entity(_entity), attribute(_attribute), value(_value) {}
    bool operator<(const Triple& other) const {
        for(ArchitectureType i = 0; i < 3; ++i)
            if(pos[i] < other.pos[i]) return true;
            else if(pos[i] > other.pos[i]) return false;
        return false;
    }
    Triple reordered(ArchitectureType to) {
        ArchitectureType alpha[] = { 0, 1, 2, 0, 1, 2 };
        ArchitectureType  beta[] = { 1, 2, 0, 2, 0, 1 };
        ArchitectureType gamma[] = { 2, 0, 1, 1, 2, 0 };
        return {pos[alpha[to]], pos[beta[to]], pos[gamma[to]]};
    }
    Triple normalized(ArchitectureType from) {
        ArchitectureType alpha[] = { 0, 2, 1, 0, 1, 2 };
        ArchitectureType  beta[] = { 1, 0, 2, 2, 0, 1 };
        ArchitectureType gamma[] = { 2, 1, 0, 1, 2, 0 };
        return {pos[alpha[from]], pos[beta[from]], pos[gamma[from]]};
    }
};
const uint64_t ArchitectureSize = sizeof(ArchitectureType)*8;

#define PreDefWrapper(token) PreDef_##token
enum PreDefSymbols {
#include "PreDefSymbols.hpp"
};
#undef PreDefWrapper

#define PreDefWrapper(token) #token
const char* PreDefSymbols[] = {
#include "PreDefSymbols.hpp"
};
#undef PreDefWrapper

#define getSymbolByName(Name) \
    Symbol Name##Symbol = task.getGuaranteed(task.block, PreDef_##Name);

#define getSymbolAndExtendByName(Name) \
    getSymbolByName(Name) \
    Extend* Name##Extend = task.context->getExtend(Name##Symbol);

#define checkExtendType(Name, expectedType) \
if(task.getGuaranteed(Name##Symbol, PreDef_Extend) != expectedType) \
    task.throwException("Invalid Extend Type");

#define getUncertainSymbolAndExtendByName(Name, DefaultValue) \
    Symbol Name##Symbol; ArchitectureType Name##Value; \
    if(task.getUncertain(task.block, PreDef_##Name, Name##Symbol)) { \
        checkExtendType(Name, PreDef_Natural) \
        Name##Value = task.get<ArchitectureType>(task.context->getExtend(Name##Symbol)); \
    } else \
        Name##Value = DefaultValue;
