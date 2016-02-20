#include "../Interpreter/PreDefProcedures.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

Symbol createSymbolFromStream(Context& context, std::istream& stream) {
    stream.seekg(0, std::ios::end);
    ArchitectureType len = stream.tellg();
    stream.seekg(0, std::ios::beg);
    Symbol symbol = context.create({{PreDef_BlobType, PreDef_Text}});
    SymbolObject* symbolObject = context.getSymbolObject(symbol);
    symbolObject->allocateBlob(len*8);
    stream.read(reinterpret_cast<char*>(symbolObject->blobData.get()), len);
    return symbol;
}

Symbol createSymbolFromFile(Context& context, std::string path) {
    std::ifstream file(path);
    return (file.good()) ? createSymbolFromStream(context, file) : PreDef_Void;
}

void debugPrintSymbol(SymbolObject* symbolObject, std::ostream& stream = std::cout) {
    stream.write(reinterpret_cast<const char*>(symbolObject->blobData.get()), symbolObject->blobSize/8);
}
