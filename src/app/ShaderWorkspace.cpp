#include "ShaderWorkspace.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
std::string makeDisplayName(const std::string& hint, int nodeId) {
    if (!hint.empty()) {
        return hint;
    }
    return "Shader " + std::to_string(nodeId);
}
}

std::string ShaderWorkspace::Document::tabLabel() const {
    std::string label = displayName.empty() ? "Shader" : displayName;
    if (dirty) {
        label += " *";
    }
    return label;
}

void ShaderWorkspace::Document::syncPathInput() {
    pendingPathInput = hasPath() ? filePath.string() : std::string{};
}

ShaderWorkspace::ShaderWorkspace() = default;

ShaderWorkspace::Document& ShaderWorkspace::createDocument(int nodeId, VkShaderStageFlagBits stage, const std::string& nameHint, const std::string& initialSource) {
    auto document = std::make_unique<Document>();
    document->nodeId = nodeId;
    document->stage = stage;
    document->displayName = makeDisplayName(nameHint.empty() ? stageDisplayName(stage) : nameHint, nodeId);
    document->editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    document->editor.SetText(initialSource.empty() ? defaultTemplateForStage(stage) : initialSource);
    document->dirty = false;
    document->compileSucceeded = false;
    document->syncPathInput();
    document->version = 1;

    auto* resultPtr = document.get();
    documents.push_back(std::move(document));
    setActiveDocument(nodeId);
    return *resultPtr;
}

void ShaderWorkspace::removeDocument(int nodeId) {
    documents.erase(std::remove_if(documents.begin(), documents.end(), [&](const std::unique_ptr<Document>& doc) {
        return doc->nodeId == nodeId;
    }), documents.end());

    if (activeDocument && activeDocument->nodeId == nodeId) {
        activeDocument = documents.empty() ? nullptr : documents.front().get();
    }
}

ShaderWorkspace::Document* ShaderWorkspace::getDocumentByNode(int nodeId) {
    for (auto& doc : documents) {
        if (doc->nodeId == nodeId) {
            return doc.get();
        }
    }
    return nullptr;
}

const ShaderWorkspace::Document* ShaderWorkspace::getDocumentByNode(int nodeId) const {
    for (const auto& doc : documents) {
        if (doc->nodeId == nodeId) {
            return doc.get();
        }
    }
    return nullptr;
}

ShaderWorkspace::Document* ShaderWorkspace::getActiveDocument() {
    return activeDocument;
}

const ShaderWorkspace::Document* ShaderWorkspace::getActiveDocument() const {
    return activeDocument;
}

void ShaderWorkspace::setActiveDocument(int nodeId) {
    if (activeDocument && activeDocument->nodeId == nodeId) {
        return;
    }

    activeDocument = getDocumentByNode(nodeId);
}

bool ShaderWorkspace::save(Document& document, std::string* outError) {
    if (!document.hasPath()) {
        if (outError) {
            *outError = "No path specified";
        }
        return false;
    }

    std::ofstream file(document.filePath, std::ios::binary);
    if (!file.is_open()) {
        if (outError) {
            *outError = "Failed to open file for writing";
        }
        return false;
    }

    auto text = document.editor.GetText();
    file << text;
    file.close();

    if (!file) {
        if (outError) {
            *outError = "Failed to write shader to disk";
        }
        return false;
    }

    document.dirty = false;
    document.hasPendingSave = false;
    document.version++;
    document.syncPathInput();
    return true;
}

bool ShaderWorkspace::saveAs(Document& document, const std::filesystem::path& path, std::string* outError) {
    document.filePath = path;
    document.displayName = path.filename().string();
    document.syncPathInput();
    return save(document, outError);
}

ShaderCompiler::Result ShaderWorkspace::compile(Document& document, const ShaderCompiler& compiler) {
    ShaderCompiler::Result result = compiler.compileGlsl(document.editor.GetText(), document.stage,
                                                         document.hasPath() ? document.filePath.string() : document.displayName,
                                                         document.entryPoint,
                                                         document.includeDirectories);

    if (result.success) {
        document.compileSucceeded = true;
        document.compileMessage = result.message;
        document.spirv = result.spirv;
    } else {
        document.compileSucceeded = false;
        document.spirv.clear();
        document.compileMessage = result.message;
    }

    return result;
}

std::string ShaderWorkspace::defaultTemplateForStage(VkShaderStageFlagBits stage) {
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return R"(#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec3 outNormal;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 viewProj;
} sceneData;

void main() {
    gl_Position = sceneData.viewProj * vec4(inPosition, 1.0);
    outNormal = inNormal;
}
)";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return R"(#version 460
layout(location = 0) in vec3 outNormal;
layout(location = 0) out vec4 outColor;

void main() {
    float lighting = max(dot(normalize(outNormal), vec3(0, 0, 1)), 0.0);
    outColor = vec4(vec3(0.3 + lighting * 0.7), 1.0);
}
)";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return R"(#version 460
layout(local_size_x = 8, local_size_y = 8) in;

void main() {
}
)";
        default:
            return "#version 460\nvoid main() {}\n";
    }
}

std::string ShaderWorkspace::stageDisplayName(VkShaderStageFlagBits stage) {
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return "Vertex";
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return "Fragment";
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return "Compute";
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return "Geometry";
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return "Tessellation Control";
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return "Tessellation Eval";
        default:
            return "Shader";
    }
}

