#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "TextEditor.h"
#include "shader/ShaderCompiler.h"

class ShaderWorkspace {
public:
    struct Document {
        int nodeId = -1;
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
        TextEditor editor;
        std::filesystem::path filePath;
        std::string pendingPathInput;
        std::string displayName;
        std::string compileMessage;
        std::string entryPoint = "main";
        std::vector<uint32_t> spirv;
        std::vector<std::filesystem::path> includeDirectories;
        bool dirty = false;
        bool compileSucceeded = false;
        bool hasPendingSave = false;
        bool autoCompile = false;
        uint64_t version = 0;

        std::string tabLabel() const;
        bool hasPath() const { return !filePath.empty(); }
        void syncPathInput();
    };

    ShaderWorkspace();

    Document& createDocument(int nodeId, VkShaderStageFlagBits stage, const std::string& nameHint, const std::string& initialSource = "");
    void removeDocument(int nodeId);

    Document* getDocumentByNode(int nodeId);
    const Document* getDocumentByNode(int nodeId) const;

    std::vector<std::unique_ptr<Document>>& getDocuments() { return documents; }
    const std::vector<std::unique_ptr<Document>>& getDocuments() const { return documents; }

    Document* getActiveDocument();
    const Document* getActiveDocument() const;
    void setActiveDocument(int nodeId);

    bool save(Document& document, std::string* outError = nullptr);
    bool saveAs(Document& document, const std::filesystem::path& path, std::string* outError = nullptr);
    ShaderCompiler::Result compile(Document& document, const ShaderCompiler& compiler);

private:
    std::vector<std::unique_ptr<Document>> documents;
    Document* activeDocument = nullptr;

    static std::string defaultTemplateForStage(VkShaderStageFlagBits stage);
    static std::string stageDisplayName(VkShaderStageFlagBits stage);
};

