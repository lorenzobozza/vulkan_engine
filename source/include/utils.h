//
//  utils.h
//  vulkan_engine
//
//  Created by Lorenzo Bozza on 11/11/21.
//

#ifndef utils_hpp
#define utils_hpp

#ifndef UTILS_IMPL
template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest);

unsigned ctz(int n);
#else
template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hashCombine(seed, rest), ...);
};

unsigned ctz(int n) {
    unsigned bits = 0, x = n;
    if (x) {
        /* mask the 8 low order bits, add 8 and shift them out if they are all 0 */
        if (!(x & 0x000000FF)) { bits +=  8; x >>=  8; }
        /* mask the 4 low order bits, add 4 and shift them out if they are all 0 */
        if (!(x & 0x0000000F)) { bits +=  4; x >>=  4; }
        /* mask the 2 low order bits, add 2 and shift them out if they are all 0 */
        if (!(x & 0x00000003)) { bits +=  2; x >>=  2; }
        /* mask the low order bit and add 1 if it is 0 */
        bits += (x & 1) ^ 1;
    }
    return bits;
}
#endif

#endif /* utils_hpp */
