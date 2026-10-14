#include <polyhook2/OwnedMemory.hpp>
int main() {
 PLH::OwnedMemory::OwnedMapping memory(1);
 auto* data=static_cast<unsigned char*>(memory.data());data[0]=42;
 const PLH::OwnedMemory::ReadRange range{data,1};
 PLH::OwnedMemory::ReadLease lease(std::span(&range,1));
 const auto spans=lease.spans();
 return spans.size()==1 && spans[0].data==data && spans[0].size==1 && spans[0].data[0]==42 ? 0 : 1;
}
