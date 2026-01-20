import urllib.request

url = "https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h"
output_file = "./src/vk_mem_alloc.h"

urllib.request.urlretrieve(url, output_file)
