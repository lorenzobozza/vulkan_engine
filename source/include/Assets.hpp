//
//  Assets.hpp
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 27/02/26.
//

#ifndef Assets_hpp
#define Assets_hpp

#include "Material.hpp"
#include "IrradianceVolume.hpp"

struct Assets {
    Assets() { materials.emplace("Global_Default_Material", Material()); }
    std::vector<std::unique_ptr<const Texture>> textures;
    std::unordered_map<std::string, Material> materials;
    std::unique_ptr<IrradianceVolume> volumeProbes;
    std::atomic_flag changed, busy;
};

#endif
