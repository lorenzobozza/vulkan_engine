//
//  utils.h
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 11/11/21.
//

#ifndef utils_hpp
#define utils_hpp

template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hashCombine(seed, rest), ...);
};

#endif /* utils_hpp */
