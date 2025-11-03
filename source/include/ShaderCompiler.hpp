//
//  ShaderCompiler.hpp
//  
//
//  Created by Lorenzo Bozza on 15/10/25.
//

#ifndef ShaderCompiler_h
#define ShaderCompiler_h

#include <shaderc/shaderc.hpp>
#include <fstream>

#include "Log.hpp"

class ShaderIncluderInterface : public shaderc::CompileOptions::IncluderInterface {
public:
    shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth) override {
    
        const std::string name = std::string(requested_source);
        std::string source;
        
        std::ifstream file;
        file.open("../../../shaders/" + name, std::ios::ate | std::ios::binary);
            
        if (file.rdstate() == std::ios::goodbit && file.is_open()) {
            size_t filesize = static_cast<size_t>(file.tellg());
            source.resize(filesize);
            
            file.seekg(0);
            file.read(source.data(), filesize);
            file.close();
        }

        auto container = new std::array<std::string, 2>;
        (*container)[0] = std::move(name);
        (*container)[1] = std::move(source);

        auto data = new shaderc_include_result;

        data->user_data = container;

        data->source_name = (*container)[0].data();
        data->source_name_length = (*container)[0].size();

        data->content = (*container)[1].data();
        data->content_length = (*container)[1].size();
    
        return data;
    }

    void ReleaseInclude(shaderc_include_result* data) override {
        delete static_cast<std::array<std::string, 2>*>(data->user_data);
        delete data;
    }
    
    ~ShaderIncluderInterface() = default;
};


class ShaderCompiler {
public:
    ShaderCompiler() = default;
    ~ShaderCompiler() = default;
    
    enum State {
        Valid = 0,
        Error
    };
    
    int loadShader(const std::string& fileName, std::vector<uint32_t>& output) {
        Log* log = Log::getInstance();
        
        // Check for cached spv
        std::ifstream file;
        file.open("../../../shaders/cache/" + fileName + ".spv", std::ios::ate | std::ios::binary);
        
        if (file.rdstate() == std::ios::goodbit && file.is_open()) {
            size_t filesize = static_cast<size_t>(file.tellg());
            output.resize(filesize / sizeof(uint32_t));
            
            file.seekg(0);
            file.read((char*)output.data(), filesize);
            file.close();
            
            return State::Valid;
        }

        file.open("../../../shaders/" + fileName, std::ios::ate | std::ios::binary);
        
        if (file.rdstate() == std::ios::goodbit && file.is_open()) {
            size_t filesize = static_cast<size_t>(file.tellg());
            source.resize(filesize);
            
            file.seekg(0);
            file.read(source.data(), filesize);
            file.close();
            
            std::string fformat = fileName.substr(fileName.size() - 4, fileName.size() - 1);
            
            ShaderCompileInfo sinfo {
                .binary = output,
                .fileName = fileName,
                .kind = (fformat == "vert") ? shaderc_vertex_shader : shaderc_fragment_shader
            };
            
            sinfo.options.SetOptimizationLevel(shaderc_optimization_level_performance);
            sinfo.options.SetIncluder(std::make_unique<ShaderIncluderInterface>());
            
            return compileShader(sinfo);
        }
        else {
            log->error("Shader: The file {} was not found", fileName);
        }
        
        return State::Error;
    }
    
private:
    struct ShaderCompileInfo {
        std::vector<uint32_t>& binary;
        const std::string& fileName;
        shaderc_shader_kind kind;
        shaderc::CompileOptions options;
    };
    
    std::vector<char> source;
    
    int compileShader(ShaderCompileInfo& info) {
        Log* log = Log::getInstance();
        
        shaderc::Compiler compiler;
        
        shaderc::PreprocessedSourceCompilationResult result;
        result = compiler.PreprocessGlsl(source.data(), source.size(), info.kind, info.fileName.c_str(), info.options);
        
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            log->error("GLSL Preprocessing: {}", result.GetErrorMessage());
            return State::Error;
        }
        
        shaderc::SpvCompilationResult output;
        output = compiler.CompileGlslToSpv(result.cbegin(), result.cend() - result.cbegin(), info.kind, info.fileName.c_str(), info.options);
        
        if (output.GetCompilationStatus() != shaderc_compilation_status_success) {
            log->error("GLSL Compilation: {}", output.GetErrorMessage());
            return State::Error;
        }
        
        size_t size = output.cend() - output.cbegin();
        info.binary.resize(size);
        
        memcpy((char*)info.binary.data(), (char*)output.cbegin(), size * sizeof(uint32_t));
        
        if (info.binary.data()[0] == 0x07230203U) {
            std::ofstream cache;
            cache.open("../../../shaders/cache/" + info.fileName + ".spv", std::ios::binary);
            cache.write((char*)info.binary.data(), info.binary.size() * sizeof(uint32_t));
            cache.close();
            
            return State::Valid;
        }
        
        log->error("Shader: {} wrong Spir-V magic number", info.fileName);
        return State::Error;
    }

};

#endif /* ShaderCompiler_h */
