#pragma once

#include <dxcapi.h>
#include <d3dcompiler.h>

class ShaderCompiler
{
public:
    ShaderCompiler();

    ~ShaderCompiler();

    ComPtr<IDxcBlob> CompileFromSource(const std::vector<char>& source);
    ComPtr<IDxcBlob> CompileFromFile(const std::string& file);
private:

    ComPtr<IDxcUtils> mUtils;
    ComPtr<IDxcCompiler3> mCompiler;
    ComPtr<IDxcIncludeHandler> mIncludeHandler;
};


