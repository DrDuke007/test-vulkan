#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

#include <filesystem>
#include <fstream>

Vec<u8> read_file(const std::filesystem::path &path)
{
    std::ifstream file{path, std::ios::binary};
    if (file.fail())
    {
        throw std::runtime_error(std::string("Could not open \"" + path.string() + "\" file!").c_str());
    }

    std::streampos begin;
    std::streampos end;
    begin = file.tellg();
    file.seekg(0, std::ios::end);
    end = file.tellg();

    Vec<u8> result(static_cast<usize>(end - begin));
    if (result.empty())
    {
        throw std::runtime_error(std::string("\"" + path.string() + "\" has a size of 0!").c_str());
    }

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(result.data()), end - begin);
    file.close();

    return result;
}


namespace vulkan
{
Handle<Shader> Device::create_shader(std::string_view path)
{
    auto bytecode = read_file(path);

    VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize                 = bytecode.size();
    info.pCode                    = reinterpret_cast<const u32 *>(bytecode.data());

    VkShaderModule vkhandle = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &vkhandle));

    return shaders.add({
            .filename = std::string(path),
            .vkhandle = vkhandle,
            .bytecode = std::move(bytecode),
        });
}

void Device::destroy_shader(Handle<Shader> shader_handle)
{
    if (auto *shader = shaders.get(shader_handle))
    {
        vkDestroyShaderModule(device, shader->vkhandle, nullptr);
        shaders.remove(shader_handle);
    }
}
};
