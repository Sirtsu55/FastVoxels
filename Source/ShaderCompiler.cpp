#include "ShaderCompiler.h"
#include "FileRead.h"
#include <filesystem>
#include <algorithm>

ShaderCompiler::ShaderCompiler()
{
    DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&mUtils));
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&mCompiler));
    mUtils->CreateDefaultIncludeHandler(&mIncludeHandler);
}

ComPtr<IDxcBlob> ShaderCompiler::CompileFromSource(const std::vector<char>& source)
{
    ComPtr<IDxcBlobEncoding> pSource;
    mUtils->CreateBlob(source.data(), source.size(), CP_UTF8, &pSource);

    // Preprocess the shader
    std::vector<const wchar_t*> arguments;

    arguments.push_back(L"-T");
    arguments.push_back(L"lib_6_6");

    arguments.push_back(L"-E");
    arguments.push_back(L"main");

    arguments.push_back(L"-enable-16bit-types");

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = pSource->GetBufferPointer();
    sourceBuffer.Size = pSource->GetBufferSize();
    sourceBuffer.Encoding = 0;

    ComPtr<IDxcResult> pCompileResult;
    mCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), mIncludeHandler.Get(),
                       IID_PPV_ARGS(&pCompileResult));

    // Error Handling
    ComPtr<IDxcBlobUtf8> pErrors;
    pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);

    if (pErrors && pErrors->GetStringLength() > 0)
    {
        std::printf("DXC Error: %s", pErrors->GetStringPointer());
    }

    ComPtr<IDxcBlob> pDxil;
    pCompileResult->GetResult(&pDxil);

    if (pDxil->GetBufferSize() == 0)
    {
        std::printf("Failed to compile shader");
        return {};
    }

    return pDxil;
}

ComPtr<IDxcBlob> ShaderCompiler::CompileFromFile(const std::string& file)
{
    std::vector<char> shaderCode;
    FileRead(file, shaderCode);
    return CompileFromSource(shaderCode);
}

ShaderCompiler::~ShaderCompiler()
{
}
